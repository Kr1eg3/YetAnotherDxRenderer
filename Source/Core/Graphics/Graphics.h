#pragma once

#include <UploadBuffer.h>
#include "../../Platform/Windows/Win32Window.h"
#include <DirectXMath.h> // in Math helper?
#include <ResourceManager.h>
#include <StaticMesh.h>
#include <FrameResource.h>


class Graphics {
public:
    Graphics(Window* wnd);
    ~Graphics() = default;

    Graphics(const Graphics&) = delete;
    Graphics& operator=(const Graphics&) = delete;
    Graphics(Graphics&&) = delete;
    Graphics& operator=(Graphics&&) = delete;

	void Update(float32 deltaTime);
	void UpdateCamera(float32 deltaTime);
    void DrawFrame();

    void OnMouseDown(MouseButton button, int32 x, int32 y);
    void OnMouseUp(MouseButton button, int32 x, int32 y);
    void OnMouseMove(int32 x, int32 y);
	void OnKeyDown(KeyCode keyCode);

	void OnResize();

private:
	void PickObject(int32 x, int32 y);
    void CreateDevice();
    void CacheDescSizes();
    void CreateSwapChain();
    void CreateRTVandDSVdescHeaps();
    void CheckMSAAqual();
    void EnableDebugLayer();
	void FlushCommandQueue();
    void CreateCommandObjects();

    ID3D12Resource* CurrentBackBuffer() const;
	D3D12_CPU_DESCRIPTOR_HANDLE DepthStencilView() const;
    D3D12_CPU_DESCRIPTOR_HANDLE CurrentBackBufferView() const;

    // Need to store this functions in Sceen class later
    void BuildPSOs();
	void BuildDefaultPSO();
	void BuildWireframePSO();
	void BuildOutlinePSO();
    void BuildShadersAndInputLayout();
	void BuildFrameResources();
    void DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const Vector<UniquePtr<RenderItem>>& ritems);

    std::array<const CD3DX12_STATIC_SAMPLER_DESC, 6> GetStaticSamplers();

private:
	Window* m_window = nullptr;

	UniquePtr<ResourceManager> m_resourceManager;

	// Frame Resources for CPU-GPU parallelism
	static const int NumFrameResources = 3;
	Vector<UniquePtr<FrameResource>> m_frameResources;
	FrameResource* m_currFrameResource = nullptr;
	int m_currFrameResourceIndex = 0;

	int m_currBackBuffer = 0;
    static const int m_swapChainBufferCount = 2;
    ComPtr<ID3D12Resource> m_swapChainBuffer[m_swapChainBufferCount];
    ComPtr<ID3D12Resource> m_depthStencilBuffer;

	ComPtr<ID3D12PipelineState> m_PSO = nullptr; // For now it's a default opaque PSO
	ComPtr<ID3D12PipelineState> m_wireframePSO = nullptr;
	ComPtr<ID3D12PipelineState> m_outlinePSO = nullptr;
	bool m_isWireframe = false;

    // Core D3D12 objects
    ComPtr<ID3D12Device> m_device;
    ComPtr<IDXGISwapChain> m_swapChain;
    ComPtr<IDXGIFactory4> m_dxgiFactory;

    ComPtr<ID3D12CommandQueue> m_commandQueue;
    ComPtr<ID3D12CommandAllocator> m_directCmdListAlloc;  // For initialization and resize
    ComPtr<ID3D12GraphicsCommandList> m_commandList;

    ComPtr<ID3D12DescriptorHeap> m_rtvHeap;
    ComPtr<ID3D12DescriptorHeap> m_dsvHeap;
    ComPtr<ID3D12DescriptorHeap> m_cbvHeap = nullptr;

	// Set true to use 4X MSAA (�4.1.8).  The default is false.
    bool m_4xMsaaState = false;    // 4X MSAA enabled
    UINT m_4xMsaaQuality = 0;      // quality level of 4X MSAA

	UINT m_rtvDescriptorSize = 0;
	UINT m_dsvDescriptorSize = 0;
	UINT m_cbvSrvUavDescriptorSize = 0;

    ComPtr<ID3DBlob> m_vsByteCode = nullptr;
    ComPtr<ID3DBlob> m_psByteCode = nullptr;
    ComPtr<ID3DBlob> m_outlineVsByteCode = nullptr;
    ComPtr<ID3DBlob> m_outlinePsByteCode = nullptr;
    std::vector<D3D12_INPUT_ELEMENT_DESC> m_inputLayout;

    D3D12_VIEWPORT m_screenViewport;
    D3D12_RECT m_scissorRect;

    ComPtr<ID3D12Fence> m_fence;
    UINT64 m_currentFence = 0;

    DXGI_FORMAT m_backBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
    DXGI_FORMAT m_depthStencilFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

    DirectX::XMFLOAT4X4 mWorld = Identity4x4();
    DirectX::XMFLOAT4X4 mView = Identity4x4();
    DirectX::XMFLOAT4X4 mProj = Identity4x4();

	DirectX::XMFLOAT3 m_eyePos = { 0.0f, 0.0f, 0.0f };

    float mTheta = 1.5f*DirectX::XM_PI;
    float mPhi = DirectX::XM_PIDIV4;
    float mRadius = 15.0f;

    POINT m_lastMousePos;

    // Object selection
    int m_selectedObjectIndex = -1;  // Index of selected render item

};
