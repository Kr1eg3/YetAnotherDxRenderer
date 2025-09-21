#include "Graphics.h"


Graphics::Graphics(Window* wnd)
	: m_window(wnd) {
	Platform::OutputDebugMessage("Initializing Graphics...\n");

	/* Code to initialize Direct3D 12. Code in this section can be
	*  similar across many applications. In Dx12 book by Frank Luna
	*  this code can be found in D3DApp::Initialize function and it
	*  never changes between his samples.
	*/
#if defined(DEBUG) || defined(_DEBUG)
	EnableDebugLayer();
#endif
	THROW_IF_FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&m_dxgiFactory)), __FUNCTION__);

	CreateDevice();

	THROW_IF_FAILED(m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE,
		IID_PPV_ARGS(&m_fence)), __FUNCTION__);

	// Descriptor sizes can vary across GPUs so we need to query
	// this information. We cache the descriptor sizes so that it
	// is available when we need it for various descriptor types.
	CacheDescSizes();

    // Check 4X MSAA quality support for our back buffer format.
    // All Direct3D 11 capable devices support 4X MSAA for all render
    // target formats, so we only need to check quality support.
	CheckMSAAqual();

	// Creation of command queue, command allocator and command list
	CreateCommandObjects();

	CreateSwapChain();

	// Create rtv and dsv descriptor heaps
	CreateRTVandDSVdescHeaps();

    // Do the initial resize code.
    OnResize();

    // Reset the command list to prep for initialization commands.
    ThrowIfFailed(m_commandList->Reset(m_directCmdListAlloc.Get(), nullptr));
	/*******************************************************************************/

	/*	This is the place where most of the sample specific code. In future i thought
	*	this should be moved in Sceene abstaction class. App should have something like
	*   SceneManager which manages multiple scenes. Each scene should create it's own
	*   resources, PSOs, descriptor heaps etc.
	*/

	// Build the descriptor heaps for the scene.
    D3D12_DESCRIPTOR_HEAP_DESC cbvHeapDesc;
    cbvHeapDesc.NumDescriptors = 2; // 1 CBV + 1 SRV for texture
    cbvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    cbvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	cbvHeapDesc.NodeMask = 0;
    ThrowIfFailed(m_device->CreateDescriptorHeap(&cbvHeapDesc,
        IID_PPV_ARGS(&m_cbvHeap)));

	// Initialize ResourceManager
	m_resourceManager = UniquePtr<ResourceManager>(
		new ResourceManager(m_device.Get(), m_commandList.Get()));

	// Build shaders and input layout
	BuildShadersAndInputLayout();

	// Build frame resources after ResourceManager is created so we know how many objects we have
	BuildFrameResources();

	BuildPSOs();
	/*******************************************************************************/

    // Execute the initialization commands.
    ThrowIfFailed(m_commandList->Close());
	ID3D12CommandList* cmdsLists[] = { m_commandList.Get() };
	m_commandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

    // Wait until initialization is complete.
    FlushCommandQueue();
}

void Graphics::BuildShadersAndInputLayout() {
	m_vsByteCode = d3dUtil::CompileShader(L"Shaders\\texture.hlsl", nullptr, "VS", "vs_5_0");
	m_psByteCode = d3dUtil::CompileShader(L"Shaders\\texture.hlsl", nullptr, "PS", "ps_5_0");
	m_inputLayout = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};
}

void Graphics::CacheDescSizes() {
	m_rtvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	m_dsvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
	m_cbvSrvUavDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
}

void Graphics::CreateRTVandDSVdescHeaps() {
	// RTV descriptors describe render target resources
	D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc;
	rtvHeapDesc.NumDescriptors = m_swapChainBufferCount;
	rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	rtvHeapDesc.NodeMask = 0;
	THROW_IF_FAILED(m_device->CreateDescriptorHeap(
		&rtvHeapDesc, IID_PPV_ARGS(m_rtvHeap.GetAddressOf())), __FUNCTION__);

	//  DSV descriptors describe depth/stencil resources
	D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc;
	dsvHeapDesc.NumDescriptors = 1;
	dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	dsvHeapDesc.NodeMask = 0;
	THROW_IF_FAILED(m_device->CreateDescriptorHeap(
		&dsvHeapDesc, IID_PPV_ARGS(m_dsvHeap.GetAddressOf())), __FUNCTION__);
}

void Graphics::CreateSwapChain() {
	assert(m_window && "Window is null");
	HWND hWnd = static_cast<HWND>(m_window->GetNativeHandle());
	assert(hWnd && "HWND is null");
	m_swapChain.Reset();

	DXGI_SWAP_CHAIN_DESC sd;
	sd.BufferDesc.Width = m_window->GetWidth();
	sd.BufferDesc.Height = m_window->GetHeight();
	sd.BufferDesc.RefreshRate.Numerator = 60;
	sd.BufferDesc.RefreshRate.Denominator = 1;
	sd.BufferDesc.Format = m_backBufferFormat;
	sd.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
	sd.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
	sd.SampleDesc.Count = m_4xMsaaState ? 4 : 1;
	sd.SampleDesc.Quality = m_4xMsaaState ? (m_4xMsaaQuality - 1) : 0;
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	sd.BufferCount = m_swapChainBufferCount;
	sd.OutputWindow = hWnd;
	sd.Windowed = true;
	sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

	// Note: Swap chain uses queue to perform flush.
	THROW_IF_FAILED(m_dxgiFactory->CreateSwapChain(
		m_commandQueue.Get(),
		&sd,
		m_swapChain.GetAddressOf()), __FUNCTION__);
}

void Graphics::CreateCommandObjects() {
	D3D12_COMMAND_QUEUE_DESC queueDesc = {}; // Command queue
	queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	THROW_IF_FAILED(m_device->CreateCommandQueue(&queueDesc,
		IID_PPV_ARGS(&m_commandQueue)), __FUNCTION__);

	// Create main command allocator for initialization and resize operations
	THROW_IF_FAILED(m_device->CreateCommandAllocator(
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		IID_PPV_ARGS(m_directCmdListAlloc.GetAddressOf())), __FUNCTION__);

	THROW_IF_FAILED(m_device->CreateCommandList(
		0,
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		m_directCmdListAlloc.Get(), // Associated command allocator
		nullptr,                    // Initial PipelineStateObject
		IID_PPV_ARGS(m_commandList.GetAddressOf())), __FUNCTION__);

	// Start off in a closed state.  This is because the first time we refer
	// to the command list we will Reset it, and it needs to be closed before
	// calling Reset.
	m_commandList->Close();
}

void Graphics::CheckMSAAqual() {
	D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS msQualityLevels;
	msQualityLevels.Format = m_backBufferFormat;
	msQualityLevels.SampleCount = 4;
	msQualityLevels.Flags = D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE;
	msQualityLevels.NumQualityLevels = 0;
	THROW_IF_FAILED(m_device->CheckFeatureSupport(
		D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS,
		&msQualityLevels,
		sizeof(msQualityLevels)), __FUNCTION__);
	m_4xMsaaQuality = msQualityLevels.NumQualityLevels;
	assert(m_4xMsaaQuality > 0 && "Unexpected MSAA quality level.");
}

void Graphics::CreateDevice() {
	ComPtr<IDXGIAdapter1> adapter;
	for (UINT adapterIndex = 0;
		SUCCEEDED(m_dxgiFactory->EnumAdapters1(adapterIndex, &adapter));
		++adapterIndex) {

		DXGI_ADAPTER_DESC1 desc;
		adapter->GetDesc1(&desc);

		// Skip software adapters
		if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) {
			continue;
		}

		// Try to create device with this adapter
		if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&m_device)))) {
			Platform::OutputDebugMessage("D3D12 device created successfully\n");
		}
	}
}

void Graphics::EnableDebugLayer() {
	ComPtr<ID3D12Debug> debugController;
	THROW_IF_FAILED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)), __FUNCTION__);
	debugController->EnableDebugLayer();
}

void Graphics::FlushCommandQueue() {
	// Advance the fence value to mark commands up to this fence point.
    m_currentFence++;

    // Add an instruction to the command queue to set a new fence point.  Because we
	// are on the GPU timeline, the new fence point won't be set until the GPU finishes
	// processing all the commands prior to this Signal().
    ThrowIfFailed(m_commandQueue->Signal(m_fence.Get(), m_currentFence));

	// Wait until the GPU has completed commands up to this fence point.
    if(m_fence->GetCompletedValue() < m_currentFence) {
		ScopedHandle eventHandle(CreateEventEx(nullptr, nullptr, false, EVENT_ALL_ACCESS));

        // Fire event when GPU hits current fence.
        ThrowIfFailed(m_fence->SetEventOnCompletion(m_currentFence, eventHandle.get()));

        // Wait until the GPU hits current fence event is fired.
		WaitForSingleObject(eventHandle.get(), INFINITE);
        // CloseHandle is automatically called by ScopedHandle destructor
	}
}

void Graphics::Update(float32 deltaTime) {
	UpdateCamera(deltaTime);

	// Cycle through the circular frame resource array.
	m_currFrameResourceIndex = (m_currFrameResourceIndex + 1) % NumFrameResources;
	m_currFrameResource = m_frameResources[m_currFrameResourceIndex].get();

	// Has the GPU finished processing the commands of the current frame resource?
	// If not, wait until the GPU has completed commands up to this fence point.
	if (m_currFrameResource->Fence != 0 && m_fence->GetCompletedValue() < m_currFrameResource->Fence) {
		HANDLE eventHandle = CreateEventEx(nullptr, nullptr, 0, EVENT_ALL_ACCESS);
		assert(eventHandle && "Failed to create event handle.");
		ThrowIfFailed(m_fence->SetEventOnCompletion(m_currFrameResource->Fence, eventHandle));
		WaitForSingleObject(eventHandle, INFINITE);
		CloseHandle(eventHandle);
	}

	// Update object constant buffers for all render items
	DirectX::XMMATRIX view = DirectX::XMLoadFloat4x4(&mView);
	DirectX::XMMATRIX proj = DirectX::XMLoadFloat4x4(&mProj);

	const auto& renderItems = m_resourceManager->GetAllRenderItems();
	for (const auto& ri : renderItems) {
		// Only update the cbuffer data if the constants have changed
		if (ri->NumFramesDirty > 0) {
			DirectX::XMMATRIX world = DirectX::XMLoadFloat4x4(&ri->World);
			DirectX::XMMATRIX texTransform = DirectX::XMLoadFloat4x4(&ri->TexTransform);

			ObjectConstants objConstants;
			DirectX::XMStoreFloat4x4(&objConstants.World, DirectX::XMMatrixTranspose(world));
			DirectX::XMStoreFloat4x4(&objConstants.TexTransform, DirectX::XMMatrixTranspose(texTransform));

			m_currFrameResource->ObjectCB->CopyData(ri->ObjCBIndex, objConstants);

			// Decrement the dirty counter
			ri->NumFramesDirty--;
		}
	}

	// Update Pass constant buffer
	if (m_currFrameResource->PassCB) {
		PassConstants passConstants;
		DirectX::XMStoreFloat4x4(&passConstants.View, DirectX::XMMatrixTranspose(view));
		DirectX::XMVECTOR viewDet = DirectX::XMMatrixDeterminant(view);
		DirectX::XMStoreFloat4x4(&passConstants.InvView, DirectX::XMMatrixTranspose(DirectX::XMMatrixInverse(&viewDet, view)));
		DirectX::XMStoreFloat4x4(&passConstants.Proj, DirectX::XMMatrixTranspose(proj));
		DirectX::XMVECTOR projDet = DirectX::XMMatrixDeterminant(proj);
		DirectX::XMStoreFloat4x4(&passConstants.InvProj, DirectX::XMMatrixTranspose(DirectX::XMMatrixInverse(&projDet, proj)));
		DirectX::XMMATRIX viewProj = view * proj;
		DirectX::XMStoreFloat4x4(&passConstants.ViewProj, DirectX::XMMatrixTranspose(viewProj));
		DirectX::XMVECTOR viewProjDet = DirectX::XMMatrixDeterminant(viewProj);
		DirectX::XMStoreFloat4x4(&passConstants.InvViewProj, DirectX::XMMatrixTranspose(DirectX::XMMatrixInverse(&viewProjDet, viewProj)));
		passConstants.EyePosW = m_eyePos;
		passConstants.RenderTargetSize = DirectX::XMFLOAT2((float)m_window->GetWidth(), (float)m_window->GetHeight());
		passConstants.InvRenderTargetSize = DirectX::XMFLOAT2(1.0f / m_window->GetWidth(), 1.0f / m_window->GetHeight());
		passConstants.NearZ = 1.0f;
		passConstants.FarZ = 1000.0f;
		passConstants.TotalTime = 0.0f; // You might want to track total time
		passConstants.DeltaTime = deltaTime;

		m_currFrameResource->PassCB->CopyData(0, passConstants);
	}
}

void Graphics::UpdateCamera(float32 deltaTime) {
	// Convert Spherical to Cartesian coordinates.
	m_eyePos.x = mRadius*sinf(mPhi)*cosf(mTheta);
	m_eyePos.z = mRadius*sinf(mPhi)*sinf(mTheta);
	m_eyePos.y = mRadius*cosf(mPhi);

	// Build the view matrix.
	DirectX::XMVECTOR pos = DirectX::XMVectorSet(m_eyePos.x, m_eyePos.y, m_eyePos.z, 1.0f);
	DirectX::XMVECTOR target = DirectX::XMVectorZero();
	DirectX::XMVECTOR up = DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

	DirectX::XMMATRIX view = DirectX::XMMatrixLookAtLH(pos, target, up);
	XMStoreFloat4x4(&mView, view);
}

void Graphics::DrawFrame() {
	auto cmdListAlloc = m_currFrameResource->CmdListAlloc;

    // Reuse the memory associated with command recording.
    // We can only reset when the associated command lists have finished execution on the GPU.
	ThrowIfFailed(cmdListAlloc->Reset());

	// A command list can be reset after it has been added to the command queue via ExecuteCommandList.
    // Reusing the command list reuses memory.
	ID3D12PipelineState* currentPSO = m_isWireframe ? m_wireframePSO.Get() : m_PSO.Get();
    ThrowIfFailed(m_commandList->Reset(cmdListAlloc.Get(), currentPSO));

    m_commandList->RSSetViewports(1, &m_screenViewport);
    m_commandList->RSSetScissorRects(1, &m_scissorRect);

    // Indicate a state transition on the resource usage.
	CD3DX12_RESOURCE_BARRIER transitionBarrier = CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
		D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
	m_commandList->ResourceBarrier(1, &transitionBarrier);

    // Clear the back buffer and depth buffer.
    m_commandList->ClearRenderTargetView(CurrentBackBufferView(), DirectX::Colors::LightSteelBlue, 0, nullptr);
    m_commandList->ClearDepthStencilView(DepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

    // Specify the buffers we are going to render to.
	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = CurrentBackBufferView();
	D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = DepthStencilView();
	m_commandList->OMSetRenderTargets(1, &rtvHandle, true, &dsvHandle);

	// Use texture atlas descriptor heap from ResourceManager
	auto textureAtlas = m_resourceManager->GetTextureAtlas();
	if (textureAtlas && textureAtlas->SrvDescriptorHeap) {
		ID3D12DescriptorHeap* descriptorHeaps[] = { textureAtlas->SrvDescriptorHeap.Get() };
		m_commandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);
	}

	const auto rootSignature = m_resourceManager->GetRootSignature();
	m_commandList->SetGraphicsRootSignature(rootSignature.Get());

	// Bind Pass constants (slot 2)
	auto passCB = m_currFrameResource->PassCB->Resource();
	m_commandList->SetGraphicsRootConstantBufferView(2, passCB->GetGPUVirtualAddress());

	// Draw all render items from ResourceManager
	const auto& renderItems = m_resourceManager->GetAllRenderItems();
	DrawRenderItems(m_commandList.Get(), renderItems);

    // Indicate a state transition on the resource usage.
	CD3DX12_RESOURCE_BARRIER presentBarrier = CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
		D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
	m_commandList->ResourceBarrier(1, &presentBarrier);

    // Done recording commands.
	ThrowIfFailed(m_commandList->Close());

    // Add the command list to the queue for execution.
	ID3D12CommandList* cmdsLists[] = { m_commandList.Get() };
	m_commandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	// swap the back and front buffers
	ThrowIfFailed(m_swapChain->Present(0, 0));
	m_currBackBuffer = (m_currBackBuffer + 1) % m_swapChainBufferCount;

	// Advance the fence value to mark commands up to this fence point.
	m_currFrameResource->Fence = ++m_currentFence;

	// Add an instruction to the command queue to set a new fence point.
	// Because we are on the GPU timeline, the new fence point won't be
	// set until the GPU finishes processing all the commands prior to this Signal().
	m_commandQueue->Signal(m_fence.Get(), m_currentFence);
}

void Graphics::OnResize() {
	assert(m_device);
	assert(m_swapChain);
	assert(m_directCmdListAlloc);

	// Flush before changing any resources.
	FlushCommandQueue();

	ThrowIfFailed(m_commandList->Reset(m_directCmdListAlloc.Get(), nullptr));

	// Release the previous resources we will be recreating.
	for (int i = 0; i < m_swapChainBufferCount; ++i) {
		m_swapChainBuffer[i].Reset();
	}
    m_depthStencilBuffer.Reset();

	// Resize the swap chain.
    ThrowIfFailed(m_swapChain->ResizeBuffers(
		m_swapChainBufferCount,
		m_window->GetWidth(), m_window->GetHeight(),
		m_backBufferFormat,
		DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH));

	m_currBackBuffer = 0;

	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHeapHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart());
	for (UINT i = 0; i < m_swapChainBufferCount; i++) {
		ThrowIfFailed(m_swapChain->GetBuffer(i, IID_PPV_ARGS(&m_swapChainBuffer[i])));
		m_device->CreateRenderTargetView(m_swapChainBuffer[i].Get(), nullptr, rtvHeapHandle);
		rtvHeapHandle.Offset(1, m_rtvDescriptorSize);
	}

    // Create the depth/stencil buffer and view.
    D3D12_RESOURCE_DESC depthStencilDesc;
    depthStencilDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    depthStencilDesc.Alignment = 0;
    depthStencilDesc.Width = m_window->GetWidth();
    depthStencilDesc.Height = m_window->GetHeight();
    depthStencilDesc.DepthOrArraySize = 1;
    depthStencilDesc.MipLevels = 1;

	// Correction 11/12/2016: SSAO chapter requires an SRV to the depth buffer to read from
	// the depth buffer.  Therefore, because we need to create two views to the same resource:
	//   1. SRV format: DXGI_FORMAT_R24_UNORM_X8_TYPELESS
	//   2. DSV Format: DXGI_FORMAT_D24_UNORM_S8_UINT
	// we need to create the depth buffer resource with a typeless format.
	depthStencilDesc.Format = DXGI_FORMAT_R24G8_TYPELESS;

    depthStencilDesc.SampleDesc.Count = m_4xMsaaState ? 4 : 1;
    depthStencilDesc.SampleDesc.Quality = m_4xMsaaState ? (m_4xMsaaQuality - 1) : 0;
    depthStencilDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    depthStencilDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

    D3D12_CLEAR_VALUE optClear;
    optClear.Format = m_depthStencilFormat;
    optClear.DepthStencil.Depth = 1.0f;
    optClear.DepthStencil.Stencil = 0;
	CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_DEFAULT);
    ThrowIfFailed(m_device->CreateCommittedResource(
        &heapProps,
		D3D12_HEAP_FLAG_NONE,
        &depthStencilDesc,
		D3D12_RESOURCE_STATE_COMMON,
        &optClear,
        IID_PPV_ARGS(m_depthStencilBuffer.GetAddressOf())));

    // Create descriptor to mip level 0 of entire resource using the format of the resource.
	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc;
	dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	dsvDesc.Format = m_depthStencilFormat;
	dsvDesc.Texture2D.MipSlice = 0;
    m_device->CreateDepthStencilView(m_depthStencilBuffer.Get(), &dsvDesc, DepthStencilView());

    // Transition the resource from its initial state to be used as a depth buffer.
	CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
		m_depthStencilBuffer.Get(),
		D3D12_RESOURCE_STATE_COMMON,
		D3D12_RESOURCE_STATE_DEPTH_WRITE
	);
	m_commandList->ResourceBarrier(1, &barrier);

    // Execute the resize commands.
    ThrowIfFailed(m_commandList->Close());
    ID3D12CommandList* cmdsLists[] = { m_commandList.Get() };
    m_commandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	// Wait until resize is complete.
	FlushCommandQueue();

	// Update the viewport transform to cover the client area.
	m_screenViewport.TopLeftX = 0;
	m_screenViewport.TopLeftY = 0;
	m_screenViewport.Width    = static_cast<float>(m_window->GetWidth());
	m_screenViewport.Height   = static_cast<float>(m_window->GetHeight());
	m_screenViewport.MinDepth = 0.0f;
	m_screenViewport.MaxDepth = 1.0f;

    m_scissorRect = { 0, 0, static_cast<LONG>(m_window->GetWidth()),
		static_cast<LONG>(m_window->GetHeight()) };

	// Update the aspect ratio and recompute the projection matrix.
	DirectX::XMMATRIX P = DirectX::XMMatrixPerspectiveFovLH(0.25f*DirectX::XM_PI,
		static_cast<float>(m_window->GetWidth()) / m_window->GetHeight(), 1.0f, 1000.0f);
	XMStoreFloat4x4(&mProj, P);
}

D3D12_CPU_DESCRIPTOR_HANDLE Graphics::DepthStencilView() const {
	return m_dsvHeap->GetCPUDescriptorHandleForHeapStart();
}

ID3D12Resource* Graphics::CurrentBackBuffer() const {
	return m_swapChainBuffer[m_currBackBuffer].Get();
}

D3D12_CPU_DESCRIPTOR_HANDLE Graphics::CurrentBackBufferView() const {
	return CD3DX12_CPU_DESCRIPTOR_HANDLE(
		m_rtvHeap->GetCPUDescriptorHandleForHeapStart(),
		m_currBackBuffer,
		m_rtvDescriptorSize);
}

void Graphics::OnMouseDown(MouseButton button, int32 x, int32 y) {
    m_lastMousePos.x = x;
    m_lastMousePos.y = y;

    SetCapture(static_cast<HWND>(m_window->GetNativeHandle()));
}

void Graphics::OnMouseUp(MouseButton button, int32 x, int32 y) {
    ReleaseCapture();
}

void Graphics::OnMouseMove(int32 x, int32 y) {
    if (m_window->IsMouseButtonPressed(MouseButton::Left)) {
        // Make each pixel correspond to a quarter of a degree.
        float dx = DirectX::XMConvertToRadians(0.25f * static_cast<float>(x - m_lastMousePos.x));
        float dy = DirectX::XMConvertToRadians(0.25f * static_cast<float>(y - m_lastMousePos.y));

        // Update angles based on input to orbit camera around box.
        mTheta += dx;
        mPhi += dy;

        // Restrict the angle mPhi.
        mPhi = std::clamp(mPhi, 0.1f, DirectX::XM_PI - 0.1f);
    }
    else if (m_window->IsMouseButtonPressed(MouseButton::Right)) {
        // Make each pixel correspond to 0.05 unit in the scene.
        float dx = 0.05f * static_cast<float>(x - m_lastMousePos.x);
        float dy = 0.05f * static_cast<float>(y - m_lastMousePos.y);

        // Update the camera radius based on input.
        mRadius += dx - dy;

        // Restrict the radius.
        mRadius = std::clamp(mRadius, 5.0f, 150.0f);
    }

    m_lastMousePos.x = x;
    m_lastMousePos.y = y;
}

void Graphics::OnKeyDown(KeyCode keyCode) {
	if (keyCode == KeyCode::Num1) {
		m_isWireframe = !m_isWireframe;
	}
}

void Graphics::BuildDefaultPSO() {
	const auto rootSignature = m_resourceManager->GetRootSignature();
    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc;
    ZeroMemory(&psoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
    psoDesc.InputLayout = { m_inputLayout.data(), (UINT)m_inputLayout.size() };
    psoDesc.pRootSignature = rootSignature.Get();
    psoDesc.VS =
	{
		reinterpret_cast<BYTE*>(m_vsByteCode->GetBufferPointer()),
		m_vsByteCode->GetBufferSize()
	};
    psoDesc.PS =
	{
		reinterpret_cast<BYTE*>(m_psByteCode->GetBufferPointer()),
		m_psByteCode->GetBufferSize()
	};
    psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    psoDesc.SampleMask = UINT_MAX;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = m_backBufferFormat;
    psoDesc.SampleDesc.Count = m_4xMsaaState ? 4 : 1;
    psoDesc.SampleDesc.Quality = m_4xMsaaState ? (m_4xMsaaQuality - 1) : 0;
    psoDesc.DSVFormat = m_depthStencilFormat;
    ThrowIfFailed(m_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_PSO)));

	m_resourceManager->AddPSO("default", m_PSO);
}

void Graphics::BuildWireframePSO() {
	const auto rootSignature = m_resourceManager->GetRootSignature();
    D3D12_GRAPHICS_PIPELINE_STATE_DESC wireframePsoDesc;
    ZeroMemory(&wireframePsoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
    wireframePsoDesc.InputLayout = { m_inputLayout.data(), (UINT)m_inputLayout.size() };
    wireframePsoDesc.pRootSignature = rootSignature.Get();
    wireframePsoDesc.VS =
	{
		reinterpret_cast<BYTE*>(m_vsByteCode->GetBufferPointer()),
		m_vsByteCode->GetBufferSize()
	};
    wireframePsoDesc.PS =
	{
		reinterpret_cast<BYTE*>(m_psByteCode->GetBufferPointer()),
		m_psByteCode->GetBufferSize()
	};
    wireframePsoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    wireframePsoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    wireframePsoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    wireframePsoDesc.SampleMask = UINT_MAX;
    wireframePsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    wireframePsoDesc.NumRenderTargets = 1;
    wireframePsoDesc.RTVFormats[0] = m_backBufferFormat;
    wireframePsoDesc.SampleDesc.Count = m_4xMsaaState ? 4 : 1;
    wireframePsoDesc.SampleDesc.Quality = m_4xMsaaState ? (m_4xMsaaQuality - 1) : 0;
    wireframePsoDesc.DSVFormat = m_depthStencilFormat;

    // Only change the fill mode to wireframe
    wireframePsoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;

    ThrowIfFailed(m_device->CreateGraphicsPipelineState(&wireframePsoDesc, IID_PPV_ARGS(&m_wireframePSO)));

	m_resourceManager->AddPSO("wireframe", m_wireframePSO);
}

void Graphics::BuildPSOs() {
	BuildDefaultPSO();
	BuildWireframePSO();
}

void Graphics::BuildFrameResources() {
	// Get the number of render items from ResourceManager
	const auto& renderItems = m_resourceManager->GetAllRenderItems();
	UINT numObjects = static_cast<UINT>(renderItems.size());

	// Ensure we have at least 1 object for safety
	if (numObjects == 0) {
		numObjects = 1;
	}

	for (int i = 0; i < NumFrameResources; ++i) {
		m_frameResources.push_back(UniquePtr<FrameResource>(
			new FrameResource(m_device.Get(),
				1,         // 1 pass CB
				numObjects, // Number of object CBs based on render items
				1)         // 1 material CB
		));
	}
}

std::array<const CD3DX12_STATIC_SAMPLER_DESC, 6> Graphics::GetStaticSamplers() {
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

void Graphics::DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const Vector<UniquePtr<RenderItem>>& ritems) {
    if (!cmdList || !m_currFrameResource || !m_currFrameResource->ObjectCB) {
        return;
    }

    UINT objCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));

    for (size_t i = 0; i < ritems.size(); ++i) {
        auto ri = ritems[i].get();
        if (!ri || !ri->Geo) {
            continue;
        }

        auto vbv = ri->Geo->VertexBufferView();
        auto ibv = ri->Geo->IndexBufferView();
        cmdList->IASetVertexBuffers(0, 1, &vbv);
        cmdList->IASetIndexBuffer(&ibv);
        cmdList->IASetPrimitiveTopology(ri->PrimitiveType);

        // Bind the texture SRV descriptor table (slot 0) - use texture from render item
        if (ri->TextureIndex >= 0) {
            auto textureAtlas = m_resourceManager->GetTextureAtlas();
            if (textureAtlas) {
                auto texHandle = textureAtlas->GetGPUHandle(static_cast<uint32>(ri->TextureIndex));
                cmdList->SetGraphicsRootDescriptorTable(0, texHandle);
            }
        }

        // Bind the CBV directly (slot 1) - matches new root signature
        D3D12_GPU_VIRTUAL_ADDRESS cbAddress = m_currFrameResource->ObjectCB->Resource()->GetGPUVirtualAddress() + ri->ObjCBIndex * objCBByteSize;
        cmdList->SetGraphicsRootConstantBufferView(1, cbAddress);

        cmdList->DrawIndexedInstanced(ri->IndexCount, 1, ri->StartIndexLocation, ri->BaseVertexLocation, 0);
    }
}
