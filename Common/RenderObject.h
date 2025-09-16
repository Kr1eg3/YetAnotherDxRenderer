#pragma once

#include <UploadBuffer.h>

struct ObjectConstants {
    DirectX::XMFLOAT4X4 WorldViewProj;
    DirectX::XMFLOAT4X4 World;
};

class IRenderObject {
public:
    virtual ~IRenderObject() = default;

    virtual void Update(float deltaTime, const DirectX::XMMATRIX& view, const DirectX::XMMATRIX& proj) = 0;
    virtual void Render(ID3D12GraphicsCommandList* cmdList) = 0;

    virtual bool IsVisible() const { return m_isVisible; }
    virtual void SetVisible(bool visible) { m_isVisible = visible; }

    virtual bool IsTransparent() const { return m_isTransparent; }

    const DirectX::XMFLOAT4X4& GetWorldMatrix() const { return m_world; }
    void SetWorldMatrix(const DirectX::XMFLOAT4X4& world) { m_world = world; }

    const DirectX::XMFLOAT3& GetPosition() const { return m_position; }
    void SetPosition(const DirectX::XMFLOAT3& pos) {
        m_position = pos;
        UpdateWorldMatrix();
    }

    const DirectX::XMFLOAT3& GetRotation() const { return m_rotation; }
    void SetRotation(const DirectX::XMFLOAT3& rot) {
        m_rotation = rot;
        UpdateWorldMatrix();
    }

    const DirectX::XMFLOAT3& GetScale() const { return m_scale; }
    void SetScale(const DirectX::XMFLOAT3& scale) {
        m_scale = scale;
        UpdateWorldMatrix();
    }

protected:
    DirectX::XMFLOAT4X4 m_world;
    DirectX::XMFLOAT3 m_position = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 m_rotation = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 m_scale = { 1.0f, 1.0f, 1.0f };

    bool m_isVisible = true;
    bool m_isTransparent = false;
    bool m_isDirty = true;

    void UpdateWorldMatrix() {
        using namespace DirectX;

        XMMATRIX scale = XMMatrixScaling(m_scale.x, m_scale.y, m_scale.z);
        XMMATRIX rotation = XMMatrixRotationRollPitchYaw(m_rotation.x, m_rotation.y, m_rotation.z);
        XMMATRIX translation = XMMatrixTranslation(m_position.x, m_position.y, m_position.z);

        XMMATRIX world = scale * rotation * translation;
        XMStoreFloat4x4(&m_world, world);
        m_isDirty = true;
    }
};

template<typename Derived>
class RenderObject : public IRenderObject {
public:
    RenderObject() {
        using namespace DirectX;
        XMStoreFloat4x4(&m_world, XMMatrixIdentity());
    }

    virtual ~RenderObject() = default;

    void Update(float deltaTime, const DirectX::XMMATRIX& view, const DirectX::XMMATRIX& proj) override {
        static_cast<Derived*>(this)->UpdateInternal(deltaTime);

        // Only update constants if we have our own CB (not using FrameResource)
        if (m_objectCB) {
            UpdateConstants(view, proj);
        }
        m_isDirty = false;
        m_constantsDirty = false;
    }

    void Render(ID3D12GraphicsCommandList* cmdList) override {
        if (!IsVisible()) return;

        auto* derived = static_cast<Derived*>(this);
        derived->PrepareRender(cmdList);
        derived->BindResources(cmdList);
        derived->Draw(cmdList);
    }

    void InitializeConstantBuffer(ID3D12Device* device, uint32 elementCount = 1) {
        m_objectCB = UniquePtr<UploadBuffer<ObjectConstants>>(new UploadBuffer<ObjectConstants>(device, elementCount, true));
        m_cbElementIndex = 0;
    }

    void SetConstantBufferIndex(uint32 index) {
        m_cbElementIndex = index;
    }

protected:
    UniquePtr<UploadBuffer<ObjectConstants>> m_objectCB;
    uint32 m_cbElementIndex = 0;
    bool m_constantsDirty = true;

    void UpdateConstants(const DirectX::XMMATRIX& view, const DirectX::XMMATRIX& proj) {
        if (!m_objectCB) return;

        using namespace DirectX;

        XMMATRIX world = XMLoadFloat4x4(&m_world);
        XMMATRIX worldViewProj = world * view * proj;

        ObjectConstants objConstants;
        XMStoreFloat4x4(&objConstants.WorldViewProj, XMMatrixTranspose(worldViewProj));
        XMStoreFloat4x4(&objConstants.World, XMMatrixTranspose(world));

        m_objectCB->CopyData(m_cbElementIndex, objConstants);
    }

    D3D12_GPU_VIRTUAL_ADDRESS GetConstantBufferAddress() const {
        if (!m_objectCB) return 0;

        UINT objCBByteSize = CalcConstantBufferByteSize(sizeof(ObjectConstants));
        return m_objectCB->Resource()->GetGPUVirtualAddress() + m_cbElementIndex * objCBByteSize;
    }

private:
    static UINT CalcConstantBufferByteSize(UINT byteSize) {
        return (byteSize + 255) & ~255;
    }
};