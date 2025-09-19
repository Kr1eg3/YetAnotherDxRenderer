#pragma once

#include <UploadBuffer.h>

struct MaterialConstants {
    DirectX::XMFLOAT4 DiffuseAlbedo = { 1.0f, 1.0f, 1.0f, 1.0f };
    DirectX::XMFLOAT3 FresnelR0 = { 0.01f, 0.01f, 0.01f };
    float Roughness = 0.25f;

    DirectX::XMFLOAT4X4 MatTransform = Identity4x4();
};

class IMeshComponent {
public:
    virtual ~IMeshComponent() = default;

    virtual void BindGeometry(ID3D12GraphicsCommandList* cmdList) = 0;
    virtual const MeshRegion& GetSubmesh(const String& name) const = 0;
    virtual bool HasSubmesh(const String& name) const = 0;
    virtual const GeometryAltas* GetMeshData() const = 0;
};

class IMaterialComponent {
public:
    virtual ~IMaterialComponent() = default;

    virtual void BindMaterial(ID3D12GraphicsCommandList* cmdList, UINT rootParameterIndex) = 0;
    virtual ID3D12PipelineState* GetPSO() const = 0;
    virtual void SetPSO(ComPtr<ID3D12PipelineState> pso) = 0;

    virtual bool IsTransparent() const = 0;
    virtual void SetTransparent(bool transparent) = 0;

    virtual const MaterialConstants& GetMaterialConstants() const = 0;
    virtual void SetMaterialConstants(const MaterialConstants& constants) = 0;
};

class IMaterial {
public:
  virtual ~IMaterial() = default;

  virtual void Bind(ID3D12GraphicsCommandList* cmdList, UINT rootIndex) = 0;
  virtual void UpdateConstants() = 0;

  virtual bool IsTransparent() const = 0;
  virtual bool RequiresAlphaTest() const = 0;
  virtual int GetRenderQueue() const = 0;

  virtual const MaterialConstants& GetConstants() const = 0;
  virtual ID3D12PipelineState* GetPSO() const = 0;
};

class ITextureComponent {
public:
    virtual ~ITextureComponent() = default;

    virtual void BindTextures(ID3D12GraphicsCommandList* cmdList, UINT rootParameterIndex) = 0;
    virtual void AddTexture(const String& name, SharedPtr<Texture> texture) = 0;
    virtual SharedPtr<Texture> GetTexture(const String& name) const = 0;
    virtual bool HasTexture(const String& name) const = 0;
};

class BasicMeshComponent : public IMeshComponent {
public:
    BasicMeshComponent(SharedPtr<GeometryAltas> mesh) : m_mesh(mesh) {}

    void BindGeometry(ID3D12GraphicsCommandList* cmdList) override {
        if (!m_mesh) return;

        auto vbv = m_mesh->VertexBufferView();
        auto ibv = m_mesh->IndexBufferView();
        cmdList->IASetVertexBuffers(0, 1, &vbv);
        cmdList->IASetIndexBuffer(&ibv);
        cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    }

    const MeshRegion& GetSubmesh(const String& name) const override {
        static MeshRegion empty;
        if (!m_mesh) return empty;

        auto it = m_mesh->GeoRegionsMap.find(name);
        return (it != m_mesh->GeoRegionsMap.end()) ? it->second : empty;
    }

    bool HasSubmesh(const String& name) const override {
        return m_mesh && m_mesh->GeoRegionsMap.find(name) != m_mesh->GeoRegionsMap.end();
    }

    const GeometryAltas* GetMeshData() const override {
        return m_mesh.get();
    }

private:
    SharedPtr<GeometryAltas> m_mesh;
};

class BasicMaterialComponent : public IMaterialComponent {
public:
    BasicMaterialComponent() {
        DirectX::XMStoreFloat4x4(&m_materialConstants.MatTransform, DirectX::XMMatrixIdentity());
    }

    void BindMaterial(ID3D12GraphicsCommandList* cmdList, UINT rootParameterIndex) override {
        if (m_pso) {
            cmdList->SetPipelineState(m_pso.Get());
        }

        if (m_materialCB) {
            auto cbAddress = m_materialCB->Resource()->GetGPUVirtualAddress();
            cmdList->SetGraphicsRootConstantBufferView(rootParameterIndex, cbAddress);
        }
    }

    ID3D12PipelineState* GetPSO() const override {
        return m_pso.Get();
    }

    void SetPSO(ComPtr<ID3D12PipelineState> pso) override {
        m_pso = pso;
    }

    bool IsTransparent() const override {
        return m_isTransparent;
    }

    void SetTransparent(bool transparent) override {
        m_isTransparent = transparent;
    }

    const MaterialConstants& GetMaterialConstants() const override {
        return m_materialConstants;
    }

    void SetMaterialConstants(const MaterialConstants& constants) override {
        m_materialConstants = constants;
        UpdateMaterialBuffer();
    }

    void InitializeMaterialBuffer(ID3D12Device* device) {
        m_materialCB = UniquePtr<UploadBuffer<MaterialConstants>>(new UploadBuffer<MaterialConstants>(device, 1, true));
        UpdateMaterialBuffer();
    }

private:
    ComPtr<ID3D12PipelineState> m_pso;
    MaterialConstants m_materialConstants;
    UniquePtr<UploadBuffer<MaterialConstants>> m_materialCB;
    bool m_isTransparent = false;

    void UpdateMaterialBuffer() {
        if (m_materialCB) {
            m_materialCB->CopyData(0, m_materialConstants);
        }
    }
};