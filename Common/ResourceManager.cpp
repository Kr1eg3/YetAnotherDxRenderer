#include "ResourceManager.h"

ResourceManager::ResourceManager(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList)
    : m_device(device), m_commandList(cmdList) {
    InitializeGeoAtlas();
    InitializeTextureAtlas();
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

MeshData ResourceManager::CreateBoxMesh(float width,
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

MeshData ResourceManager::CreatePlaneMesh(float width,
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

MeshData ResourceManager::CreateSphereMesh(float radius, uint32 sliceCount, uint32 stackCount) {
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

MeshData ResourceManager::CreateCylinderMesh(float bottomRadius, float topRadius, float height,
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
    MeshData boxData = CreateBoxMesh();
    MeshData planeData = CreatePlaneMesh(20.0f, 20.0f, 4, 4);
    MeshData sphereData = CreateSphereMesh(1.0f, 20, 20);
    MeshData cylinderData = CreateCylinderMesh(0.5f, 0.5f, 3.0f, 20, 20);

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
    m_textureAtlas->InitializeDescriptorHeap(m_device, 1); // 64 textures max

    // Load default texture(s)
    auto woodCrateTex = SharedPtr<Texture>(new Texture());
    woodCrateTex->name = "woodCrate";
    woodCrateTex->filename = L"Textures/WoodCrate01.dds";

    ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(m_device,
        m_commandList, woodCrateTex->filename.c_str(),
        woodCrateTex->resource, woodCrateTex->uploadHeap));

    // Add texture to atlas (automatically creates SRV)
    AddTexture(woodCrateTex->name, woodCrateTex);
}
