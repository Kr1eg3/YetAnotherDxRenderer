#include "ResourceManager.h"

ResourceManager::ResourceManager(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList)
    : m_device(device), m_commandList(cmdList) {

    InitializeTextureAtlas();
    BuildRootSignature();
    InitializeGeoAtlas();

    BuildRenderItems();
}

void ResourceManager::AddRenderItemObject(GeometryAltas* atlas, DirectX::XMMATRIX worldTransform,
    DirectX::XMMATRIX texTransform, UINT objCBidx, int texIdx, String& meshRegionKey) {
	auto ri = std::make_unique<RenderItem>();
	DirectX::XMStoreFloat4x4(&ri->World, worldTransform);
	DirectX::XMStoreFloat4x4(&ri->TexTransform, texTransform);
	ri->ObjCBIndex = objCBidx;
	ri->TextureIndex = texIdx;
	ri->Geo = atlas;
	ri->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	const auto* meshRegion = GetMeshRegion(meshRegionKey);
	if (meshRegion) {
		ri->IndexCount = meshRegion->IndexCount;
		ri->StartIndexLocation = meshRegion->StartIndexLocation;
		ri->BaseVertexLocation = meshRegion->BaseVertexLocation;
	}
	m_allRitems.push_back(std::move(ri));
}

ComPtr<ID3D12Resource> ResourceManager::CreateDefaultBuffer(const void* initData,
                                                           UINT64 byteSize,
                                                           ComPtr<ID3D12Resource>& uploadBuffer) {
    ComPtr<ID3D12Resource> defaultBuffer;

    // Create the actual default buffer resource
    CD3DX12_HEAP_PROPERTIES defaultHeapProps(D3D12_HEAP_TYPE_DEFAULT);
    CD3DX12_RESOURCE_DESC bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(byteSize);

    m_device->CreateCommittedResource(
        &defaultHeapProps,
        D3D12_HEAP_FLAG_NONE,
        &bufferDesc,
        D3D12_RESOURCE_STATE_COMMON,
        nullptr,
        IID_PPV_ARGS(defaultBuffer.GetAddressOf()));

    // Create upload buffer
    CD3DX12_HEAP_PROPERTIES uploadHeapProps(D3D12_HEAP_TYPE_UPLOAD);

    m_device->CreateCommittedResource(
        &uploadHeapProps,
        D3D12_HEAP_FLAG_NONE,
        &bufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(uploadBuffer.GetAddressOf()));

    // Copy data to upload buffer
    D3D12_SUBRESOURCE_DATA subResourceData = {};
    subResourceData.pData = initData;
    subResourceData.RowPitch = byteSize;
    subResourceData.SlicePitch = subResourceData.RowPitch;

    // Schedule copy from upload buffer to default buffer
    CD3DX12_RESOURCE_BARRIER barrier1 = CD3DX12_RESOURCE_BARRIER::Transition(
        defaultBuffer.Get(),
        D3D12_RESOURCE_STATE_COMMON,
        D3D12_RESOURCE_STATE_COPY_DEST);
    m_commandList->ResourceBarrier(1, &barrier1);

    UpdateSubresources<1>(m_commandList, defaultBuffer.Get(), uploadBuffer.Get(),
                         0, 0, 1, &subResourceData);

    CD3DX12_RESOURCE_BARRIER barrier2 = CD3DX12_RESOURCE_BARRIER::Transition(
        defaultBuffer.Get(),
        D3D12_RESOURCE_STATE_COPY_DEST,
        D3D12_RESOURCE_STATE_GENERIC_READ);
    m_commandList->ResourceBarrier(1, &barrier2);

    return defaultBuffer;
}

ComPtr<ID3DBlob> ResourceManager::CompileShader(const String& name,
                                               const WString& filename,
                                               const String& entrypoint,
                                               const String& target) {
    UINT compileFlags = 0;
#if defined(DEBUG) || defined(_DEBUG)
    compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

    ComPtr<ID3DBlob> byteCode = nullptr;
    ComPtr<ID3DBlob> errors;

    HRESULT hr = D3DCompileFromFile(filename.c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE,
                                    entrypoint.c_str(), target.c_str(), compileFlags, 0,
                                    &byteCode, &errors);

    if (errors != nullptr) {
        OutputDebugStringA((char*)errors->GetBufferPointer());
    }

    if (SUCCEEDED(hr)) {
        m_shaders[name] = byteCode;
    }

    return byteCode;
}

ComPtr<ID3D12PipelineState> ResourceManager::CreatePSO(const String& name,
                                                      const D3D12_GRAPHICS_PIPELINE_STATE_DESC& desc) {
    ComPtr<ID3D12PipelineState> pso;
    HRESULT hr = m_device->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&pso));

    if (SUCCEEDED(hr)) {
        m_psos[name] = pso;
    }

    return pso;
}

SharedPtr<IMaterialComponent> ResourceManager::CreateMaterial(const String& psoName,
                                                                   const MaterialConstants& constants) {
    auto material = SharedPtr<BasicMaterialComponent>(new BasicMaterialComponent());

    auto pso = GetPSO(psoName);
    if (pso) {
        material->SetPSO(pso);
    }

    material->SetMaterialConstants(constants);
    material->InitializeMaterialBuffer(m_device);

    return material;
}

SharedPtr<IMeshComponent> ResourceManager::CreateMeshComponent(const String& regionName) {
    if (!m_primitiveAtlas) return nullptr;

    // Check if the region exists in our atlas
    auto regionIt = m_meshRegions.find(regionName);
    if (regionIt == m_meshRegions.end()) return nullptr;

    // Use BasicMeshComponent with the atlas
    // The component will use GeoRegionsMap to find the correct region
    return SharedPtr<BasicMeshComponent>(new BasicMeshComponent(m_primitiveAtlas));
}

void ResourceManager::InitializeGeoAtlas() {
    // Create all primitive geometries
    MeshData boxData = GeometryGenerator::CreateBoxMesh(1.0f, 1.0f, 1.0f);
    MeshData planeData = GeometryGenerator::CreatePlaneMesh(20.0f, 30.0f, 60, 40);
    MeshData sphereData = GeometryGenerator::CreateSphereMesh(0.5f, 20, 20);
    MeshData cylinderData = GeometryGenerator::CreateCylinderMesh(0.5f, 0.3f, 3.0f, 20, 20);

    // Calculate offsets for each mesh in the combined buffer
    uint32 currentVertexOffset = 0;
    uint32 currentIndexOffset = 0;

    // Store regions for each mesh
    MeshRegion boxRegion;
    boxRegion.BaseVertexLocation = currentVertexOffset;
    boxRegion.StartIndexLocation = currentIndexOffset;
    boxRegion.IndexCount = (UINT)boxData.Indices32.size();
    m_meshRegions["box"] = boxRegion;

    currentVertexOffset += (uint32)boxData.Vertices.size();
    currentIndexOffset += (uint32)boxData.Indices32.size();

    MeshRegion planeRegion;
    planeRegion.BaseVertexLocation = currentVertexOffset;
    planeRegion.StartIndexLocation = currentIndexOffset;
    planeRegion.IndexCount = (UINT)planeData.Indices32.size();
    m_meshRegions["plane"] = planeRegion;

    currentVertexOffset += (uint32)planeData.Vertices.size();
    currentIndexOffset += (uint32)planeData.Indices32.size();

    MeshRegion sphereRegion;
    sphereRegion.BaseVertexLocation = currentVertexOffset;
    sphereRegion.StartIndexLocation = currentIndexOffset;
    sphereRegion.IndexCount = (UINT)sphereData.Indices32.size();
    m_meshRegions["sphere"] = sphereRegion;

    currentVertexOffset += (uint32)sphereData.Vertices.size();
    currentIndexOffset += (uint32)sphereData.Indices32.size();

    MeshRegion cylinderRegion;
    cylinderRegion.BaseVertexLocation = currentVertexOffset;
    cylinderRegion.StartIndexLocation = currentIndexOffset;
    cylinderRegion.IndexCount = (UINT)cylinderData.Indices32.size();
    m_meshRegions["cylinder"] = cylinderRegion;

    // Combine all vertices and indices (using 16-bit indices like in reference)
    Vector<Vertex> allVertices;
    Vector<uint16> allIndices;

    // Add box vertices and indices
    allVertices.insert(allVertices.end(), boxData.Vertices.begin(), boxData.Vertices.end());
    auto& boxIndices16 = boxData.GetIndices16();
    allIndices.insert(allIndices.end(), boxIndices16.begin(), boxIndices16.end());

    // Add plane vertices and indices (no index offset needed - BaseVertexLocation handles it)
    allVertices.insert(allVertices.end(), planeData.Vertices.begin(), planeData.Vertices.end());
    auto& planeIndices16 = planeData.GetIndices16();
    allIndices.insert(allIndices.end(), planeIndices16.begin(), planeIndices16.end());

    // Add sphere vertices and indices
    allVertices.insert(allVertices.end(), sphereData.Vertices.begin(), sphereData.Vertices.end());
    auto& sphereIndices16 = sphereData.GetIndices16();
    allIndices.insert(allIndices.end(), sphereIndices16.begin(), sphereIndices16.end());

    // Add cylinder vertices and indices
    allVertices.insert(allVertices.end(), cylinderData.Vertices.begin(), cylinderData.Vertices.end());
    auto& cylinderIndices16 = cylinderData.GetIndices16();
    allIndices.insert(allIndices.end(), cylinderIndices16.begin(), cylinderIndices16.end());

    // Create the atlas
    m_primitiveAtlas = SharedPtr<GeometryAltas>(new GeometryAltas());
    m_primitiveAtlas->Name = "PrimitiveAtlas";

    const UINT vbByteSize = (UINT)allVertices.size() * sizeof(Vertex);
    const UINT ibByteSize = (UINT)allIndices.size() * sizeof(uint16);

    // Create CPU memory buffers
    D3DCreateBlob(vbByteSize, &m_primitiveAtlas->VertexBufferCPU);
    CopyMemory(m_primitiveAtlas->VertexBufferCPU->GetBufferPointer(), allVertices.data(), vbByteSize);

    D3DCreateBlob(ibByteSize, &m_primitiveAtlas->IndexBufferCPU);
    CopyMemory(m_primitiveAtlas->IndexBufferCPU->GetBufferPointer(), allIndices.data(), ibByteSize);

    // Create GPU buffers
    m_primitiveAtlas->VertexBufferGPU = CreateDefaultBuffer(allVertices.data(), vbByteSize, m_primitiveAtlas->VertexBufferUploader);
    m_primitiveAtlas->IndexBufferGPU = CreateDefaultBuffer(allIndices.data(), ibByteSize, m_primitiveAtlas->IndexBufferUploader);

    m_primitiveAtlas->VertexByteStride = sizeof(Vertex);
    m_primitiveAtlas->VertexBufferByteSize = vbByteSize;
    m_primitiveAtlas->IndexFormat = DXGI_FORMAT_R16_UINT;
    m_primitiveAtlas->IndexBufferByteSize = ibByteSize;

    // Store all regions in the atlas for compatibility
    m_primitiveAtlas->GeoRegionsMap = m_meshRegions;
}

void ResourceManager::InitializeTextureAtlas() {
    // Create texture atlas
    m_textureAtlas = SharedPtr<TextureAtlas>(new TextureAtlas());
    m_textureAtlas->Name = "MainTextureAtlas";

    // Initialize descriptor heap (1 CBV + multiple textures)
    m_textureAtlas->InitializeDescriptorHeap(m_device, 64); // 64 textures max

    // Load default texture(s)
    auto woodCrateTex = SharedPtr<Texture>(new Texture());
    woodCrateTex->name = "woodCrate";
    woodCrateTex->filename = L"Textures/WoodCrate01.dds";

    ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(m_device,
        m_commandList, woodCrateTex->filename.c_str(),
        woodCrateTex->resource, woodCrateTex->uploadHeap));

    AddTexture(woodCrateTex->name, woodCrateTex);

    // Load stone texture for spheres and cylinders
    auto stoneTex = SharedPtr<Texture>(new Texture());
    stoneTex->name = "stone";
    stoneTex->filename = L"Textures/stone.dds";

    ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(m_device,
        m_commandList, stoneTex->filename.c_str(),
        stoneTex->resource, stoneTex->uploadHeap));

    AddTexture(stoneTex->name, stoneTex);

    // Load tile texture for floor
    auto tileTex = SharedPtr<Texture>(new Texture());
    tileTex->name = "tile";
    tileTex->filename = L"Textures/tile.dds";

    ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(m_device,
        m_commandList, tileTex->filename.c_str(),
        tileTex->resource, tileTex->uploadHeap));

    AddTexture(tileTex->name, tileTex);

    auto grassTex = SharedPtr<Texture>(new Texture());
    grassTex->name = "grass";
    grassTex->filename = L"Textures/grass.dds";

    ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(m_device,
        m_commandList, grassTex->filename.c_str(),
        grassTex->resource, grassTex->uploadHeap));

    AddTexture(grassTex->name, grassTex);
}


void ResourceManager::BuildRenderItems() {
    // Box
    String boxKey = "box";
    AddRenderItemObject(m_primitiveAtlas.get(),
                       DirectX::XMMatrixScaling(2.0f, 2.0f, 2.0f) * DirectX::XMMatrixTranslation(0.0f, 1.0f, 0.0f),
                       DirectX::XMMatrixScaling(1.0f, 1.0f, 1.0f),
                       0, 0, boxKey);

    // Grid
    String planeKey = "plane";
    AddRenderItemObject(m_primitiveAtlas.get(),
                       DirectX::XMMatrixIdentity(),
                       DirectX::XMMatrixScaling(8.0f, 8.0f, 1.0f),
                       1, 3, planeKey);

    // Columns and spheres
    DirectX::XMMATRIX brickTexTransform = DirectX::XMMatrixScaling(1.0f, 1.0f, 1.0f);
    DirectX::XMMATRIX identityTexTransform = DirectX::XMMatrixIdentity();
    UINT objCBIndex = 2;

    String cylinderKey = "cylinder";
    String sphereKey = "sphere";

    for(int i = 0; i < 5; ++i) {
        // Left cylinder with stone texture
        AddRenderItemObject(m_primitiveAtlas.get(),
                           DirectX::XMMatrixTranslation(-5.0f, 1.5f, -10.0f + i*5.0f),
                           brickTexTransform,
                           objCBIndex++, 1, cylinderKey);

        // Right cylinder with stone texture
        AddRenderItemObject(m_primitiveAtlas.get(),
                           DirectX::XMMatrixTranslation(+5.0f, 1.5f, -10.0f + i*5.0f),
                           brickTexTransform,
                           objCBIndex++, 1, cylinderKey);

        // Left sphere with stone texture
        AddRenderItemObject(m_primitiveAtlas.get(),
                           DirectX::XMMatrixTranslation(-5.0f, 3.5f, -10.0f + i*5.0f),
                           identityTexTransform,
                           objCBIndex++, 1, sphereKey);

        // Right sphere with stone texture
        AddRenderItemObject(m_primitiveAtlas.get(),
                           DirectX::XMMatrixTranslation(+5.0f, 3.5f, -10.0f + i*5.0f),
                           identityTexTransform,
                           objCBIndex++, 1, sphereKey);
    }

    // All render items divided by PSO.
    for(auto& e : m_allRitems) {
        // For now all items use the same PSO
    }
}

void ResourceManager::BuildRootSignature() {
	// Shader programs typically require resources as input (constant buffers,
	// textures, samplers).  The root signature defines the resources the shader
	// programs expect.  If we think of the shader programs as a function, and
	// the input resources as function parameters, then the root signature can be
	// thought of as defining the function signature.

	// Root parameter can be a table, root descriptor or root constants.
	CD3DX12_ROOT_PARAMETER slotRootParameter[3];

	// Create SRV table for textures
	CD3DX12_DESCRIPTOR_RANGE texTable;
	texTable.Init(
        D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
		1,  // number of descriptors
		0); // register t0

	slotRootParameter[0].InitAsDescriptorTable(1, &texTable, D3D12_SHADER_VISIBILITY_PIXEL);
	slotRootParameter[1].InitAsConstantBufferView(0); // register b0 - Object constants
	slotRootParameter[2].InitAsConstantBufferView(1); // register b1 - Pass constants

	// Create samplers
	auto staticSamplers = GetStaticSamplers();

	// A root signature is an array of root parameters.
	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(3, slotRootParameter,
		(UINT)staticSamplers.size(), staticSamplers.data(),
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	// create a root signature with a single slot which points to a descriptor range consisting of a single constant buffer
	ComPtr<ID3DBlob> serializedRootSig = nullptr;
	ComPtr<ID3DBlob> errorBlob = nullptr;
	HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1,
		serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf());

	if (errorBlob != nullptr) {
		::OutputDebugStringA((char*)errorBlob->GetBufferPointer());
	}
	ThrowIfFailed(hr);

	ThrowIfFailed(m_device->CreateRootSignature(
		0,
		serializedRootSig->GetBufferPointer(),
		serializedRootSig->GetBufferSize(),
		IID_PPV_ARGS(&m_rootSignature)));
}

Array<const CD3DX12_STATIC_SAMPLER_DESC, 6> ResourceManager::GetStaticSamplers() {
	// Applications usually only need a handful of samplers. So just define them all up front
	// and keep them available as part of the root signature.

	const CD3DX12_STATIC_SAMPLER_DESC pointWrap(
		0, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_POINT, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC pointClamp(
		1, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_POINT, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC linearWrap(
		2, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC linearClamp(
		3, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC anisotropicWrap(
		4, // shaderRegister
		D3D12_FILTER_ANISOTROPIC, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressW
		0.0f,                              // mipLODBias
		8);                                // maxAnisotropy

	const CD3DX12_STATIC_SAMPLER_DESC anisotropicClamp(
		5, // shaderRegister
		D3D12_FILTER_ANISOTROPIC, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressW
		0.0f,                               // mipLODBias
		8);                                 // maxAnisotropy

	return { pointWrap, pointClamp, linearWrap, linearClamp, anisotropicWrap, anisotropicClamp };
}

