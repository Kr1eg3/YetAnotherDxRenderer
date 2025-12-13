//***************************************************************************************
// texture.hlsl - Simple texture mapping shader without lighting
//***************************************************************************************

Texture2D gDiffuseMap : register(t0);
SamplerState gsamPointWrap : register(s0);
SamplerState gsamPointClamp : register(s1);
SamplerState gsamLinearWrap : register(s2);
SamplerState gsamLinearClamp : register(s3);
SamplerState gsamAnisotropicWrap : register(s4);
SamplerState gsamAnisotropicClamp : register(s5);

cbuffer cbPerObject : register(b0)
{
	float4x4 gWorld;
	float4x4 gTexTransform;
};

cbuffer cbPass : register(b1)
{
    float4x4 gView;
    float4x4 gInvView;
    float4x4 gProj;
    float4x4 gInvProj;
    float4x4 gViewProj;
    float4x4 gInvViewProj;
    float3 gEyePosW;
    float cbPerObjectPad1;
    float2 gRenderTargetSize;
    float2 gInvRenderTargetSize;
    float gNearZ;
    float gFarZ;
    float gTotalTime;
    float gDeltaTime;
};

struct VertexIn
{
	float3 PosL  : POSITION;
    float3 NormalL : NORMAL;
    float2 TexC : TEXCOORD;
};

struct VertexOut
{
	float4 PosH  : SV_POSITION;
    float2 TexC : TEXCOORD;
};

VertexOut VS(VertexIn vin)
{
	VertexOut vout;

	// Transform to world space.
    float4 posW = mul(float4(vin.PosL, 1.0f), gWorld);

    // Transform to homogeneous clip space.
	vout.PosH = mul(posW, gViewProj);

	// Pass texture coordinates to pixel shader (apply texture transform)
    vout.TexC = mul(float4(vin.TexC, 0.0f, 1.0f), gTexTransform).xy;

    return vout;
}

#define PIXEL_SIZE 1.0f
#define N_COLORS 3.0f

static const float Bayer4x4[16] =
{
    0, 8, 2, 10,
    12, 4, 14, 6,
     3, 11, 1, 9,
    15, 7, 13, 5
};

float4 PS(VertexOut pin) : SV_Target
{
    float2 snapped_tex_coords = pin.TexC * gRenderTargetSize;
    snapped_tex_coords = floor(snapped_tex_coords / PIXEL_SIZE) * PIXEL_SIZE;
    snapped_tex_coords = snapped_tex_coords * gInvRenderTargetSize;
    
    float4 diffuseAlbedo = gDiffuseMap.Sample(gsamAnisotropicWrap, pin.TexC);

    float c = saturate(dot(diffuseAlbedo.rgb, float3(0.299, 0.587, 0.114)));

    int block_x_index = (int) floor(pin.PosH.x / PIXEL_SIZE);
    int block_y_index = (int) floor(pin.PosH.y / PIXEL_SIZE);
    
    int x = block_x_index % 4;
    int y = block_y_index % 4;

    float M_norm = Bayer4x4[y * 4 + x] / 16.0;

    float n_minus_1 = N_COLORS - 1.0;
    float adjusted_c = c * n_minus_1 + M_norm;
    int index = (int) floor(adjusted_c);
    float quantized_c = (float) index;
    float3 finalColor = diffuseAlbedo.rgb * quantized_c;

    return float4(finalColor, diffuseAlbedo.a);
}