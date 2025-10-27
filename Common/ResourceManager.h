#pragma once

#include "RenderComponents.h"

class RenderItem {
public:
	RenderItem() = default;
    RenderItem(const RenderItem& rhs) = delete;

    // World matrix of the shape that describes the object's local space
    // relative to the world space, which defines the position, orientation,
    // and scale of the object in the world.
    DirectX::XMFLOAT4X4 World = Identity4x4();

	DirectX::XMFLOAT4X4 TexTransform = Identity4x4();

	// Dirty flag indicating the object data has changed and we need to update the constant buffer.
	// Because we have an object cbuffer for each FrameResource, we have to apply the
	// update to each FrameResource.  Thus, when we modify obect data we should set
	// NumFramesDirty = gNumFrameResources so that each frame resource gets the update.
	int NumFramesDirty = 3;

	// Index into GPU constant buffer corresponding to the ObjectCB for this render item.
	UINT ObjCBIndex = -1;

	//Material* Mat = nullptr;
	GeometryAltas* Geo = nullptr;
	int TextureIndex = 0; // Index in texture atlas

    // Primitive topology.
    D3D12_PRIMITIVE_TOPOLOGY PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

    // DrawIndexedInstanced parameters.
    UINT IndexCount = 0;
    UINT StartIndexLocation = 0;
    int BaseVertexLocation = 0;
};

class ResourceManager {
public:
    ResourceManager(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList);

    ~ResourceManager() = default;

    void AddRenderItemObject(GeometryAltas* atlas, DirectX::XMMATRIX worldTransform,
        DirectX::XMMATRIX texTransform, UINT objCBidx, int texIdx,
        String& meshRegionKey);

    // Get the main geometry atlas containing all primitive meshes
    SharedPtr<GeometryAltas> GetPrimitiveAtlas() const {
        return m_primitiveAtlas;
    }

    // Get specific mesh region from the atlas
    const MeshRegion* GetMeshRegion(const String& name) const {
        auto it = m_meshRegions.find(name);
        return (it != m_meshRegions.end()) ? &it->second : nullptr;
    }

    // Get the texture atlas containing all textures
    SharedPtr<TextureAtlas> GetTextureAtlas() const {
        return m_textureAtlas;
    }

    // Get texture index from atlas
    int GetTextureIndex(const String& name) const {
        return m_textureAtlas ? m_textureAtlas->GetTextureIndex(name) : -1;
    }

    // Texture management
    SharedPtr<Texture> GetTexture(const String& name) {
        if (!m_textureAtlas) return nullptr;
        auto it = m_textureAtlas->Textures.find(name);
        return (it != m_textureAtlas->Textures.end()) ? it->second : nullptr;
    }

    // Add texture to the atlas (automatically creates SRV)
    uint32 AddTexture(const String& name, SharedPtr<Texture> texture) {
        if (!m_textureAtlas) return UINT32_MAX;
        return m_textureAtlas->AddTexture(m_device, name, texture);
    }

    SharedPtr<Texture> LoadTextureFromFile(const String& name,
                                                 const WString& filename);

    // Pipeline State Object management
    ComPtr<ID3D12PipelineState> GetPSO(const String& name) {
        auto it = m_psos.find(name);
        return (it != m_psos.end()) ? it->second : nullptr;
    }

    void AddPSO(const String& name, ComPtr<ID3D12PipelineState> pso) {
        m_psos[name] = pso;
    }

    ComPtr<ID3D12PipelineState> CreatePSO(const String& name,
                                          const D3D12_GRAPHICS_PIPELINE_STATE_DESC& desc);

    // Shader management
    ComPtr<ID3DBlob> GetShader(const String& name) {
        auto it = m_shaders.find(name);
        return (it != m_shaders.end()) ? it->second : nullptr;
    }

    void AddShader(const String& name, ComPtr<ID3DBlob> shader) {
        m_shaders[name] = shader;
    }

    ComPtr<ID3DBlob> CompileShader(const String& name,
                                   const WString& filename,
                                   const String& entrypoint,
                                   const String& target);

    // Material component factory
    SharedPtr<IMaterialComponent> CreateMaterial(const String& psoName,
                                                       const MaterialConstants& constants = MaterialConstants());

    // Mesh component factory - now uses regions from the atlas
    SharedPtr<IMeshComponent> CreateMeshComponent(const String& regionName);

    // Cleanup upload buffers after GPU upload is complete
    void CleanupUploadBuffers() {
        if (m_primitiveAtlas) {
            m_primitiveAtlas->DisposeUploaders();
        }

        if (m_textureAtlas) {
            m_textureAtlas->DisposeUploaders();
        }
    }

    // Get all resource names for debugging
    Vector<String> GetMeshRegionNames() const {
        Vector<String> names;
        for (const auto& [name, region] : m_meshRegions) {
            names.push_back(name);
        }
        return names;
    }

    Vector<String> GetTextureNames() const {
        Vector<String> names;
        if (m_textureAtlas) {
            for (const auto& [name, texture] : m_textureAtlas->Textures) {
                names.push_back(name);
            }
        }
        return names;
    }

    Vector<String> GetPSONames() const {
        Vector<String> names;
        for (const auto& [name, pso] : m_psos) {
            names.push_back(name);
        }
        return names;
    }

    ID3D12Device* GetDevice() const { return m_device; }
    ID3D12GraphicsCommandList* GetCommandList() const { return m_commandList; }
	ComPtr<ID3D12RootSignature> GetRootSignature() const { return m_rootSignature; }
    const Vector<UniquePtr<RenderItem>>& GetAllRenderItems() const { return m_allRitems; }

private:
    ID3D12Device* m_device;
    ID3D12GraphicsCommandList* m_commandList;
    ComPtr<ID3D12RootSignature> m_rootSignature = nullptr;

	// List of all the render items.
	Vector<UniquePtr<RenderItem>> m_allRitems;

    // Single geometry atlas containing all primitive meshes
    SharedPtr<GeometryAltas> m_primitiveAtlas;
    // Map of region names to their locations in the atlas
    HashMap<String, MeshRegion> m_meshRegions;

    // Single texture atlas containing all textures
    SharedPtr<TextureAtlas> m_textureAtlas;

    // Other resources
    HashMap<String, ComPtr<ID3D12PipelineState>> m_psos;
    HashMap<String, ComPtr<ID3DBlob>> m_shaders;

    // Helper functions
    ComPtr<ID3D12Resource> CreateDefaultBuffer(const void* initData,
                                               UINT64 byteSize,
                                               ComPtr<ID3D12Resource>& uploadBuffer);

    // Initialize the primitive atlas with all basic geometries
    void InitializeGeoAtlas();
    void InitializeTextureAtlas();

	void BuildRootSignature();
	void BuildRenderItems();

	Array<const CD3DX12_STATIC_SAMPLER_DESC, 6> GetStaticSamplers();
};