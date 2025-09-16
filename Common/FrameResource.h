#pragma once

#include <WindowsPlatform.h>
#include <RenderComponents.h>
#include <RenderObject.h>

// Forward declarations
struct MaterialConstants; // Already defined in RenderComponents.h

// PassConstants contains per-frame constant data like view/projection matrices
struct PassConstants {
    DirectX::XMFLOAT4X4 View;
    DirectX::XMFLOAT4X4 InvView;
    DirectX::XMFLOAT4X4 Proj;
    DirectX::XMFLOAT4X4 InvProj;
    DirectX::XMFLOAT4X4 ViewProj;
    DirectX::XMFLOAT4X4 InvViewProj;
    DirectX::XMFLOAT3 EyePosW = { 0.0f, 0.0f, 0.0f };
    float cbPerObjectPad1 = 0.0f;
    DirectX::XMFLOAT2 RenderTargetSize = { 0.0f, 0.0f };
    DirectX::XMFLOAT2 InvRenderTargetSize = { 0.0f, 0.0f };
    float NearZ = 0.0f;
    float FarZ = 0.0f;
    float TotalTime = 0.0f;
    float DeltaTime = 0.0f;
};

// Stores the resources needed for the CPU to build the command lists
// for a frame. Each frame has its own set of resources to avoid
// synchronization issues between CPU and GPU.
class FrameResource {
public:
    FrameResource(ID3D12Device* device, UINT passCount, UINT objectCount, UINT materialCount = 0);
    ~FrameResource();

    // We cannot copy or assign frame resources
    FrameResource(const FrameResource& rhs) = delete;
    FrameResource& operator=(const FrameResource& rhs) = delete;

    // We cannot reset the allocator until the GPU is done processing the commands.
    // So each frame needs its own allocator.
    ComPtr<ID3D12CommandAllocator> CmdListAlloc;

    // We cannot update a cbuffer until the GPU is done processing the commands
    // that reference it. So each frame needs its own cbuffers.
    UniquePtr<UploadBuffer<PassConstants>> PassCB;
    UniquePtr<UploadBuffer<ObjectConstants>> ObjectCB;
    UniquePtr<UploadBuffer<MaterialConstants>> MaterialCB;

    // Fence value to mark commands up to this fence point. This lets us
    // check if these frame resources are still in use by the GPU.
    UINT64 Fence = 0;
};