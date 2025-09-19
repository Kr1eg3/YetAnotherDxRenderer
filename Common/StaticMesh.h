#pragma once

#include "RenderObject.h"
#include "RenderComponents.h"

class StaticMesh : public RenderObject<StaticMesh> {
public:
    StaticMesh() = default;

    StaticMesh(SharedPtr<IMeshComponent> mesh,
               SharedPtr<IMaterialComponent> material,
               const String& submeshName = "default")
        : m_mesh(mesh), m_material(material), m_submeshName(submeshName) {}

    void SetMesh(SharedPtr<IMeshComponent> mesh) {
        m_mesh = mesh;
    }

    void SetMaterial(SharedPtr<IMaterialComponent> material) {
        m_material = material;
        if (material) {
            m_isTransparent = material->IsTransparent();
        }
    }

    void SetSubmeshName(const String& name) {
        m_submeshName = name;
    }

    void SetTextures(SharedPtr<ITextureComponent> textures) {
        m_textureAtlas = textures;
    }

    void UpdateInternal(float deltaTime) {
        // Can be overridden for specific update logic
        // For example: animation, rotation, etc.
        // Note: Constant buffer updates are now handled in Graphics::Update
        // using FrameResource
    }

    void PrepareRender(ID3D12GraphicsCommandList* cmdList) {
        // Set any common state before rendering
        // This can be overridden for specific preparation
    }

    void BindResources(ID3D12GraphicsCommandList* cmdList) {
        if (!m_mesh || !m_material) return;

        // Bind mesh geometry
        m_mesh->BindGeometry(cmdList);

        // For now, use descriptor table for compatibility with existing root signature
        // The constant buffer is already bound via descriptor heap in Graphics::DrawFrame

        // Don't override PSO if wireframe mode might be active
        // PSO is already set in Graphics::DrawFrame based on wireframe state
        // Only set material PSO if it's different from default (for special materials)
        // Commented out for now to allow wireframe mode to work properly
        // if (m_material->GetPSO()) {
        //     cmdList->SetPipelineState(m_material->GetPSO());
        // }

        // Note: Material constants and textures binding commented out
        // until we extend the root signature
        // m_material->BindMaterial(cmdList, 1);
        // if (m_textureAtlas) {
        //     m_textureAtlas->BindTextures(cmdList, 2);
        // }
    }

    void Draw(ID3D12GraphicsCommandList* cmdList) {
        if (!m_mesh || !m_material) return;

        if (!m_mesh->HasSubmesh(m_submeshName)) return;

        const auto& submesh = m_mesh->GetSubmesh(m_submeshName);

        cmdList->DrawIndexedInstanced(
            submesh.IndexCount,
            1,
            submesh.StartIndexLocation,
            submesh.BaseVertexLocation,
            0
        );
    }

    SharedPtr<IMeshComponent> GetMesh() const { return m_mesh; }
    SharedPtr<IMaterialComponent> GetMaterial() const { return m_material; }
    SharedPtr<ITextureComponent> GetTextures() const { return m_textureAtlas; }
    const String& GetSubmeshName() const { return m_submeshName; }

private:
    SharedPtr<IMeshComponent> m_mesh;
    SharedPtr<IMaterialComponent> m_material;
    SharedPtr<ITextureComponent> m_textureAtlas;
    String m_submeshName = "default";
};

class InstancedStaticMesh : public RenderObject<InstancedStaticMesh> {
public:
    struct InstanceData {
        DirectX::XMFLOAT4X4 World;
        DirectX::XMFLOAT4 Color;
    };

    InstancedStaticMesh() = default;

    InstancedStaticMesh(SharedPtr<IMeshComponent> mesh,
                        SharedPtr<IMaterialComponent> material,
                        const String& submeshName = "default")
        : m_mesh(mesh), m_material(material), m_submeshName(submeshName) {}

    void AddInstance(const InstanceData& instance) {
        m_instances.push_back(instance);
        m_instancesDirty = true;
    }

    void ClearInstances() {
        m_instances.clear();
        m_instancesDirty = true;
    }

    void UpdateInstanceData(size_t index, const InstanceData& data) {
        if (index < m_instances.size()) {
            m_instances[index] = data;
            m_instancesDirty = true;
        }
    }

    void InitializeInstanceBuffer(ID3D12Device* device, size_t maxInstances) {
        m_instanceBuffer = UniquePtr<UploadBuffer<InstanceData>>(
            new UploadBuffer<InstanceData>(device, static_cast<UINT>(maxInstances), false));
        m_maxInstances = maxInstances;
    }

    void UpdateInternal(float deltaTime) {
        if (m_instancesDirty && m_instanceBuffer) {
            UpdateInstanceBuffer();
            m_instancesDirty = false;
        }
    }

    void PrepareRender(ID3D12GraphicsCommandList* cmdList) {
        // Prepare for instanced rendering
    }

    void BindResources(ID3D12GraphicsCommandList* cmdList) {
        if (!m_mesh || !m_material) return;

        m_mesh->BindGeometry(cmdList);

        // For instanced rendering, we'll need to set up instance buffer
        if (m_instanceBuffer && !m_instances.empty()) {
            D3D12_VERTEX_BUFFER_VIEW instanceBufferView;
            instanceBufferView.BufferLocation = m_instanceBuffer->Resource()->GetGPUVirtualAddress();
            instanceBufferView.StrideInBytes = sizeof(InstanceData);
            instanceBufferView.SizeInBytes = static_cast<UINT>(m_instances.size() * sizeof(InstanceData));

            cmdList->IASetVertexBuffers(1, 1, &instanceBufferView);
        }

        m_material->BindMaterial(cmdList, 1);
    }

    void Draw(ID3D12GraphicsCommandList* cmdList) {
        if (!m_mesh || !m_material || m_instances.empty()) return;

        if (!m_mesh->HasSubmesh(m_submeshName)) return;

        const auto& submesh = m_mesh->GetSubmesh(m_submeshName);

        cmdList->DrawIndexedInstanced(
            submesh.IndexCount,
            static_cast<UINT>(m_instances.size()),
            submesh.StartIndexLocation,
            submesh.BaseVertexLocation,
            0
        );
    }

    size_t GetInstanceCount() const { return m_instances.size(); }
    const Vector<InstanceData>& GetInstances() const { return m_instances; }

private:
    SharedPtr<IMeshComponent> m_mesh;
    SharedPtr<IMaterialComponent> m_material;
    String m_submeshName = "default";

    Vector<InstanceData> m_instances;
    UniquePtr<UploadBuffer<InstanceData>> m_instanceBuffer;
    size_t m_maxInstances = 0;
    bool m_instancesDirty = true;

    void UpdateInstanceBuffer() {
        size_t count = std::min(m_instances.size(), m_maxInstances);
        for (size_t i = 0; i < count; ++i) {
            m_instanceBuffer->CopyData(static_cast<int>(i), m_instances[i]);
        }
    }
};