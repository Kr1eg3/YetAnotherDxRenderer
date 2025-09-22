#include "WindowsPlatform.h"
#include <codecvt>
#include <locale>
#include <fstream>

namespace Platform {

String WStringToString(const WString& wstr) {
    if (wstr.empty()) return String();

    int size = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(),
                                   nullptr, 0, nullptr, nullptr);
    String result(size, 0);
    WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(),
                        &result[0], size, nullptr, nullptr);
    return result;
}

WString StringToWString(const String& str) {
    if (str.empty()) return WString();

    int size = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(),
                                   nullptr, 0);
    WString result(size, 0);
    MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(),
                        &result[0], size);
    return result;
}

void ShowMessageBox(const String& title, const String& message) {
    WString wTitle = StringToWString(title);
    WString wMessage = StringToWString(message);
    MessageBoxW(nullptr, wMessage.c_str(), wTitle.c_str(), MB_OK | MB_ICONINFORMATION);
}

void ShowMessageBox(const HWND hwnd, const String& title, const String& message) {
    WString wTitle = StringToWString(title);
    WString wMessage = StringToWString(message);
    MessageBoxW(hwnd, wMessage.c_str(), wTitle.c_str(), MB_OK | MB_ICONINFORMATION);
}

void OutputDebugMessage(const String& message) {
    WString wMessage = StringToWString(message);
    OutputDebugStringW(wMessage.c_str());
}

} // namespace Platform

bool d3dUtil::IsKeyDown(int vkeyCode) {
    return (GetAsyncKeyState(vkeyCode) & 0x8000) != 0;
}

ComPtr<ID3DBlob> d3dUtil::LoadBinary(const std::wstring& filename) {
    std::ifstream fin(filename, std::ios::binary);

    fin.seekg(0, std::ios_base::end);
    std::ifstream::pos_type size = (int)fin.tellg();
    fin.seekg(0, std::ios_base::beg);

    ComPtr<ID3DBlob> blob;
    ThrowIfFailed(D3DCreateBlob(size, blob.GetAddressOf()));

    fin.read((char*)blob->GetBufferPointer(), size);
    fin.close();

    return blob;
}

ComPtr<ID3D12Resource> d3dUtil::CreateDefaultBuffer(
    ID3D12Device* device,
    ID3D12GraphicsCommandList* cmdList,
    const void* initData,
    UINT64 byteSize,
    Microsoft::WRL::ComPtr<ID3D12Resource>& uploadBuffer) {
    ComPtr<ID3D12Resource> defaultBuffer;

    // Create the actual default buffer resource.
    CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_DEFAULT);
    CD3DX12_RESOURCE_DESC resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(byteSize);

    ThrowIfFailed(device->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &resourceDesc,
		D3D12_RESOURCE_STATE_COMMON,
        nullptr,
        IID_PPV_ARGS(defaultBuffer.GetAddressOf())));

    // In order to copy CPU memory data into our default buffer, we need to create
    // an intermediate upload heap.
    CD3DX12_HEAP_PROPERTIES uploadHeapProps(D3D12_HEAP_TYPE_UPLOAD);
    CD3DX12_RESOURCE_DESC uploadResourceDesc = CD3DX12_RESOURCE_DESC::Buffer(byteSize);

    ThrowIfFailed(device->CreateCommittedResource(
        &uploadHeapProps,
		D3D12_HEAP_FLAG_NONE,
        &uploadResourceDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(uploadBuffer.GetAddressOf())));


    // Describe the data we want to copy into the default buffer.
    D3D12_SUBRESOURCE_DATA subResourceData = {};
    subResourceData.pData = initData;
    subResourceData.RowPitch = byteSize;
    subResourceData.SlicePitch = subResourceData.RowPitch;

    // Schedule to copy the data to the default buffer resource.  At a high level, the helper function UpdateSubresources
    // will copy the CPU memory into the intermediate upload heap.  Then, using ID3D12CommandList::CopySubresourceRegion,
    // the intermediate upload heap data will be copied to mBuffer.
	CD3DX12_RESOURCE_BARRIER barrier1 = CD3DX12_RESOURCE_BARRIER::Transition(defaultBuffer.Get(),
		D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST);
	cmdList->ResourceBarrier(1, &barrier1);
    UpdateSubresources<1>(cmdList, defaultBuffer.Get(), uploadBuffer.Get(), 0, 0, 1, &subResourceData);
	CD3DX12_RESOURCE_BARRIER barrier2 = CD3DX12_RESOURCE_BARRIER::Transition(defaultBuffer.Get(),
		D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_GENERIC_READ);
	cmdList->ResourceBarrier(1, &barrier2);

    // Note: uploadBuffer has to be kept alive after the above function calls because
    // the command list has not been executed yet that performs the actual copy.
    // The caller can Release the uploadBuffer after it knows the copy has been executed.


    return defaultBuffer;
}

ComPtr<ID3DBlob> d3dUtil::CompileShader(
	const std::wstring& filename,
	const D3D_SHADER_MACRO* defines,
	const std::string& entrypoint,
	const std::string& target) {
	UINT compileFlags = 0;
#if defined(DEBUG) || defined(_DEBUG)
	compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

	HRESULT hr = S_OK;

	ComPtr<ID3DBlob> byteCode = nullptr;
	ComPtr<ID3DBlob> errors;
	hr = D3DCompileFromFile(filename.c_str(), defines, D3D_COMPILE_STANDARD_FILE_INCLUDE,
		entrypoint.c_str(), target.c_str(), compileFlags, 0, &byteCode, &errors);

    if (errors != nullptr) {
		OutputDebugStringA((char*)errors->GetBufferPointer());
    }

	ThrowIfFailed(hr);

	return byteCode;
}

MeshData GeometryGenerator::CreateBoxMesh(float width,
                                        float height,
                                        float depth) {
    MeshData meshData;
    meshData.Vertices.resize(24);

    float w2 = width * 0.5f;
    float h2 = height * 0.5f;
    float d2 = depth * 0.5f;

    std::array<Vertex, 24> vertices = {{
        // Front face
        Vertex({ DirectX::XMFLOAT3(-w2, -h2, -d2), DirectX::XMFLOAT3(0.0f, 0.0f, -1.0f), DirectX::XMFLOAT2(0.0f, 1.0f) }),
        Vertex({ DirectX::XMFLOAT3(-w2, +h2, -d2), DirectX::XMFLOAT3(0.0f, 0.0f, -1.0f), DirectX::XMFLOAT2(0.0f, 0.0f) }),
        Vertex({ DirectX::XMFLOAT3(+w2, +h2, -d2), DirectX::XMFLOAT3(0.0f, 0.0f, -1.0f), DirectX::XMFLOAT2(1.0f, 0.0f) }),
        Vertex({ DirectX::XMFLOAT3(+w2, -h2, -d2), DirectX::XMFLOAT3(0.0f, 0.0f, -1.0f), DirectX::XMFLOAT2(1.0f, 1.0f) }),

        // Back face
        Vertex({ DirectX::XMFLOAT3(-w2, -h2, +d2), DirectX::XMFLOAT3(0.0f, 0.0f, 1.0f), DirectX::XMFLOAT2(1.0f, 1.0f) }),
        Vertex({ DirectX::XMFLOAT3(+w2, -h2, +d2), DirectX::XMFLOAT3(0.0f, 0.0f, 1.0f), DirectX::XMFLOAT2(0.0f, 1.0f) }),
        Vertex({ DirectX::XMFLOAT3(+w2, +h2, +d2), DirectX::XMFLOAT3(0.0f, 0.0f, 1.0f), DirectX::XMFLOAT2(0.0f, 0.0f) }),
        Vertex({ DirectX::XMFLOAT3(-w2, +h2, +d2), DirectX::XMFLOAT3(0.0f, 0.0f, 1.0f), DirectX::XMFLOAT2(1.0f, 0.0f) }),

        // Top face
        Vertex({ DirectX::XMFLOAT3(-w2, +h2, -d2), DirectX::XMFLOAT3(0.0f, 1.0f, 0.0f), DirectX::XMFLOAT2(0.0f, 1.0f) }),
        Vertex({ DirectX::XMFLOAT3(-w2, +h2, +d2), DirectX::XMFLOAT3(0.0f, 1.0f, 0.0f), DirectX::XMFLOAT2(0.0f, 0.0f) }),
        Vertex({ DirectX::XMFLOAT3(+w2, +h2, +d2), DirectX::XMFLOAT3(0.0f, 1.0f, 0.0f), DirectX::XMFLOAT2(1.0f, 0.0f) }),
        Vertex({ DirectX::XMFLOAT3(+w2, +h2, -d2), DirectX::XMFLOAT3(0.0f, 1.0f, 0.0f), DirectX::XMFLOAT2(1.0f, 1.0f) }),

        // Bottom face
        Vertex({ DirectX::XMFLOAT3(-w2, -h2, -d2), DirectX::XMFLOAT3(0.0f, -1.0f, 0.0f), DirectX::XMFLOAT2(1.0f, 1.0f) }),
        Vertex({ DirectX::XMFLOAT3(+w2, -h2, -d2), DirectX::XMFLOAT3(0.0f, -1.0f, 0.0f), DirectX::XMFLOAT2(0.0f, 1.0f) }),
        Vertex({ DirectX::XMFLOAT3(+w2, -h2, +d2), DirectX::XMFLOAT3(0.0f, -1.0f, 0.0f), DirectX::XMFLOAT2(0.0f, 0.0f) }),
        Vertex({ DirectX::XMFLOAT3(-w2, -h2, +d2), DirectX::XMFLOAT3(0.0f, -1.0f, 0.0f), DirectX::XMFLOAT2(1.0f, 0.0f) }),

        // Left face
        Vertex({ DirectX::XMFLOAT3(-w2, -h2, +d2), DirectX::XMFLOAT3(-1.0f, 0.0f, 0.0f), DirectX::XMFLOAT2(0.0f, 1.0f) }),
        Vertex({ DirectX::XMFLOAT3(-w2, +h2, +d2), DirectX::XMFLOAT3(-1.0f, 0.0f, 0.0f), DirectX::XMFLOAT2(0.0f, 0.0f) }),
        Vertex({ DirectX::XMFLOAT3(-w2, +h2, -d2), DirectX::XMFLOAT3(-1.0f, 0.0f, 0.0f), DirectX::XMFLOAT2(1.0f, 0.0f) }),
        Vertex({ DirectX::XMFLOAT3(-w2, -h2, -d2), DirectX::XMFLOAT3(-1.0f, 0.0f, 0.0f), DirectX::XMFLOAT2(1.0f, 1.0f) }),

        // Right face
        Vertex({ DirectX::XMFLOAT3(+w2, -h2, -d2), DirectX::XMFLOAT3(1.0f, 0.0f, 0.0f), DirectX::XMFLOAT2(0.0f, 1.0f) }),
        Vertex({ DirectX::XMFLOAT3(+w2, +h2, -d2), DirectX::XMFLOAT3(1.0f, 0.0f, 0.0f), DirectX::XMFLOAT2(0.0f, 0.0f) }),
        Vertex({ DirectX::XMFLOAT3(+w2, +h2, +d2), DirectX::XMFLOAT3(1.0f, 0.0f, 0.0f), DirectX::XMFLOAT2(1.0f, 0.0f) }),
        Vertex({ DirectX::XMFLOAT3(+w2, -h2, +d2), DirectX::XMFLOAT3(1.0f, 0.0f, 0.0f), DirectX::XMFLOAT2(1.0f, 1.0f) })
    }};

    meshData.Vertices.assign(vertices.begin(), vertices.end());

    meshData.Indices32 = {
        // Front face
        0, 1, 2,
        0, 2, 3,

        // Back face
        4, 5, 6,
        4, 6, 7,

        // Top face
        8, 9, 10,
        8, 10, 11,

        // Bottom face
        12, 13, 14,
        12, 14, 15,

        // Left face
        16, 17, 18,
        16, 18, 19,

        // Right face
        20, 21, 22,
        20, 22, 23
    };

    return meshData;
}

MeshData GeometryGenerator::CreatePlaneMesh(float width,
                                          float depth,
                                          uint32 m,
                                          uint32 n) {
    MeshData meshData;
    uint32 vertexCount = m * n;
    uint32 faceCount = (m - 1) * (n - 1) * 2;

    float halfWidth = 0.5f * width;
    float halfDepth = 0.5f * depth;

    float dx = width / (n - 1);
    float dz = depth / (m - 1);

    float du = 1.0f / (n - 1);
    float dv = 1.0f / (m - 1);

    meshData.Vertices.resize(vertexCount);
    for (uint32 i = 0; i < m; ++i) {
        float z = halfDepth - i * dz;
        for (uint32 j = 0; j < n; ++j) {
            float x = -halfWidth + j * dx;

            meshData.Vertices[i * n + j].Pos = DirectX::XMFLOAT3(x, 0.0f, z);
            meshData.Vertices[i * n + j].Normal = DirectX::XMFLOAT3(0.0f, 1.0f, 0.0f);
            meshData.Vertices[i * n + j].TexCoord = DirectX::XMFLOAT2(j * du, i * dv);
        }
    }

    meshData.Indices32.resize(faceCount * 3);

    uint32 k = 0;
    for (uint32 i = 0; i < m - 1; ++i) {
        for (uint32 j = 0; j < n - 1; ++j) {
            meshData.Indices32[k] = i * n + j;
            meshData.Indices32[k + 1] = i * n + j + 1;
            meshData.Indices32[k + 2] = (i + 1) * n + j;

            meshData.Indices32[k + 3] = (i + 1) * n + j;
            meshData.Indices32[k + 4] = i * n + j + 1;
            meshData.Indices32[k + 5] = (i + 1) * n + j + 1;

            k += 6;
        }
    }

    return meshData;
}

MeshData GeometryGenerator::CreateSphereMesh(float radius, uint32 sliceCount, uint32 stackCount) {
    MeshData meshData;

    // Compute the vertices starting at the top pole and moving down the stacks
    // Top vertex
    Vertex topVertex;
    topVertex.Pos = DirectX::XMFLOAT3(0.0f, +radius, 0.0f);
    topVertex.Normal = DirectX::XMFLOAT3(0.0f, +1.0f, 0.0f);
    topVertex.TexCoord = DirectX::XMFLOAT2(0.0f, 0.0f);
    meshData.Vertices.push_back(topVertex);

    float phiStep = DirectX::XM_PI / stackCount;
    float thetaStep = 2.0f * DirectX::XM_PI / sliceCount;

    // Compute vertices for each stack ring (do not count the poles as rings)
    for (uint32 i = 1; i <= stackCount - 1; ++i) {
        float phi = i * phiStep;

        // Vertices of ring
        for (uint32 j = 0; j <= sliceCount; ++j) {
            float theta = j * thetaStep;

            Vertex v;

            // Spherical to Cartesian
            v.Pos.x = radius * sinf(phi) * cosf(theta);
            v.Pos.y = radius * cosf(phi);
            v.Pos.z = radius * sinf(phi) * sinf(theta);

            // Normalized position vector is the normal
            DirectX::XMVECTOR p = DirectX::XMLoadFloat3(&v.Pos);
            DirectX::XMStoreFloat3(&v.Normal, DirectX::XMVector3Normalize(p));

            v.TexCoord.x = theta / DirectX::XM_2PI;
            v.TexCoord.y = phi / DirectX::XM_PI;

            meshData.Vertices.push_back(v);
        }
    }

    // Bottom vertex
    Vertex bottomVertex;
    bottomVertex.Pos = DirectX::XMFLOAT3(0.0f, -radius, 0.0f);
    bottomVertex.Normal = DirectX::XMFLOAT3(0.0f, -1.0f, 0.0f);
    bottomVertex.TexCoord = DirectX::XMFLOAT2(0.0f, 1.0f);
    meshData.Vertices.push_back(bottomVertex);

    // Compute indices for top stack
    for (uint32 i = 1; i <= sliceCount; ++i) {
        meshData.Indices32.push_back(0);
        meshData.Indices32.push_back(i + 1);
        meshData.Indices32.push_back(i);
    }

    // Compute indices for inner stacks
    uint32 baseIndex = 1;
    uint32 ringVertexCount = sliceCount + 1;
    for (uint32 i = 0; i < stackCount - 2; ++i) {
        for (uint32 j = 0; j < sliceCount; ++j) {
            meshData.Indices32.push_back(baseIndex + i * ringVertexCount + j);
            meshData.Indices32.push_back(baseIndex + i * ringVertexCount + j + 1);
            meshData.Indices32.push_back(baseIndex + (i + 1) * ringVertexCount + j);

            meshData.Indices32.push_back(baseIndex + (i + 1) * ringVertexCount + j);
            meshData.Indices32.push_back(baseIndex + i * ringVertexCount + j + 1);
            meshData.Indices32.push_back(baseIndex + (i + 1) * ringVertexCount + j + 1);
        }
    }

    // Compute indices for bottom stack
    uint32 southPoleIndex = (uint32)meshData.Vertices.size() - 1;
    baseIndex = southPoleIndex - ringVertexCount;
    for (uint32 i = 0; i < sliceCount; ++i) {
        meshData.Indices32.push_back(southPoleIndex);
        meshData.Indices32.push_back(baseIndex + i);
        meshData.Indices32.push_back(baseIndex + i + 1);
    }

    return meshData;
}

MeshData GeometryGenerator::CreateCylinderMesh(float bottomRadius, float topRadius, float height,
                                             uint32 sliceCount, uint32 stackCount) {
    MeshData meshData;

    float stackHeight = height / stackCount;
    float radiusStep = (topRadius - bottomRadius) / stackCount;
    uint32 ringCount = stackCount + 1;

    // Compute vertices for each stack ring starting at the bottom and moving up
    for (uint32 i = 0; i < ringCount; ++i) {
        float y = -0.5f * height + i * stackHeight;
        float r = bottomRadius + i * radiusStep;

        float dTheta = 2.0f * DirectX::XM_PI / sliceCount;
        for (uint32 j = 0; j <= sliceCount; ++j) {
            Vertex vertex;

            float c = cosf(j * dTheta);
            float s = sinf(j * dTheta);

            vertex.Pos = DirectX::XMFLOAT3(r * c, y, r * s);

            vertex.TexCoord.x = (float)j / sliceCount;
            vertex.TexCoord.y = 1.0f - (float)i / stackCount;

            // Cylinder side normal
            float dr = bottomRadius - topRadius;
            DirectX::XMFLOAT3 bitangent(dr * c, -height, dr * s);

            DirectX::XMVECTOR T = DirectX::XMLoadFloat3(&bitangent);
            DirectX::XMVECTOR B = DirectX::XMVectorSet(s, 0.0f, -c, 0.0f);
            DirectX::XMVECTOR N = DirectX::XMVector3Normalize(DirectX::XMVector3Cross(T, B));
            DirectX::XMStoreFloat3(&vertex.Normal, N);

            meshData.Vertices.push_back(vertex);
        }
    }

    // Add one because we duplicate the first and last vertex per ring
    uint32 ringVertexCount = sliceCount + 1;

    // Compute indices for each stack
    for (uint32 i = 0; i < stackCount; ++i) {
        for (uint32 j = 0; j < sliceCount; ++j) {
            meshData.Indices32.push_back(i * ringVertexCount + j);
            meshData.Indices32.push_back((i + 1) * ringVertexCount + j);
            meshData.Indices32.push_back((i + 1) * ringVertexCount + j + 1);

            meshData.Indices32.push_back(i * ringVertexCount + j);
            meshData.Indices32.push_back((i + 1) * ringVertexCount + j + 1);
            meshData.Indices32.push_back(i * ringVertexCount + j + 1);
        }
    }

    // Build top cap
    uint32 baseIndex = (uint32)meshData.Vertices.size();
    float y = 0.5f * height;
    float dTheta = 2.0f * DirectX::XM_PI / sliceCount;

    // Duplicate cap ring vertices because the texture coordinates and normals differ
    for (uint32 i = 0; i <= sliceCount; ++i) {
        float x = topRadius * cosf(i * dTheta);
        float z = topRadius * sinf(i * dTheta);

        float u = x / height + 0.5f;
        float v = z / height + 0.5f;

        meshData.Vertices.push_back(Vertex({ DirectX::XMFLOAT3(x, y, z), DirectX::XMFLOAT3(0.0f, 1.0f, 0.0f), DirectX::XMFLOAT2(u, v) }));
    }

    // Cap center vertex
    meshData.Vertices.push_back(Vertex({ DirectX::XMFLOAT3(0.0f, y, 0.0f), DirectX::XMFLOAT3(0.0f, 1.0f, 0.0f), DirectX::XMFLOAT2(0.5f, 0.5f) }));

    // Index of center vertex
    uint32 centerIndex = (uint32)meshData.Vertices.size() - 1;

    for (uint32 i = 0; i < sliceCount; ++i) {
        meshData.Indices32.push_back(centerIndex);
        meshData.Indices32.push_back(baseIndex + i + 1);
        meshData.Indices32.push_back(baseIndex + i);
    }

    // Build bottom cap
    baseIndex = (uint32)meshData.Vertices.size();
    y = -0.5f * height;

    // Duplicate cap ring vertices
    for (uint32 i = 0; i <= sliceCount; ++i) {
        float x = bottomRadius * cosf(i * dTheta);
        float z = bottomRadius * sinf(i * dTheta);

        float u = x / height + 0.5f;
        float v = z / height + 0.5f;

        meshData.Vertices.push_back(Vertex({ DirectX::XMFLOAT3(x, y, z), DirectX::XMFLOAT3(0.0f, -1.0f, 0.0f), DirectX::XMFLOAT2(u, v) }));
    }

    // Cap center vertex
    meshData.Vertices.push_back(Vertex({ DirectX::XMFLOAT3(0.0f, y, 0.0f), DirectX::XMFLOAT3(0.0f, -1.0f, 0.0f), DirectX::XMFLOAT2(0.5f, 0.5f) }));

    centerIndex = (uint32)meshData.Vertices.size() - 1;

    for (uint32 i = 0; i < sliceCount; ++i) {
        meshData.Indices32.push_back(centerIndex);
        meshData.Indices32.push_back(baseIndex + i);
        meshData.Indices32.push_back(baseIndex + i + 1);
    }

    return meshData;
}