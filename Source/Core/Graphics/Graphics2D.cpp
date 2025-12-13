#include "Graphics2D.h"

Graphics2D::~Graphics2D() {
    Shutdown();
}

bool Graphics2D::Initialize(
    ID3D12Device* d3d12Device,
    ID3D12CommandQueue* commandQueue,
    IDXGISwapChain* swapChain,
    UINT backBufferCount,
    DXGI_FORMAT backBufferFormat)
{
    m_backBufferCount = backBufferCount;

    // 1. Создаём D2D Factory
    D2D1_FACTORY_OPTIONS factoryOptions = {};
#if defined(_DEBUG)
    factoryOptions.debugLevel = D2D1_DEBUG_LEVEL_INFORMATION;
#endif

    HRESULT hr = D2D1CreateFactory(
        D2D1_FACTORY_TYPE_SINGLE_THREADED,
        __uuidof(ID2D1Factory3),
        &factoryOptions,
        reinterpret_cast<void**>(m_d2dFactory.GetAddressOf())
    );
    if (FAILED(hr)) {
        Platform::OutputDebugMessage("Failed to create D2D Factory\n");
        return false;
    }

    // 2. Создаём DWrite Factory
    hr = DWriteCreateFactory(
        DWRITE_FACTORY_TYPE_SHARED,
        __uuidof(IDWriteFactory3),
        reinterpret_cast<IUnknown**>(m_dwriteFactory.GetAddressOf())
    );
    if (FAILED(hr)) {
        Platform::OutputDebugMessage("Failed to create DWrite Factory\n");
        return false;
    }

    // 3. Создаём D3D11on12 Device
    ComPtr<ID3D11Device> d3d11Device;
    UINT d3d11DeviceFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#if defined(_DEBUG)
    d3d11DeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    hr = D3D11On12CreateDevice(
        d3d12Device,
        d3d11DeviceFlags,
        nullptr, 0,                                          // Feature levels (use default)
        reinterpret_cast<IUnknown**>(&commandQueue), 1,      // Command queues
        0,                                                   // Node mask
        &d3d11Device,
        &m_d3d11DeviceContext,
        nullptr
    );
    if (FAILED(hr)) {
        Platform::OutputDebugMessage("Failed to create D3D11on12 Device\n");
        return false;
    }

    // 4. Получаем D3D11On12Device интерфейс
    hr = d3d11Device.As(&m_d3d11On12Device);
    if (FAILED(hr)) {
        Platform::OutputDebugMessage("Failed to get ID3D11On12Device interface\n");
        return false;
    }

    m_d3d11Device = d3d11Device;

    // 5. Создаём D2D Device
    ComPtr<IDXGIDevice> dxgiDevice;
    hr = m_d3d11On12Device.As(&dxgiDevice);
    if (FAILED(hr)) {
        Platform::OutputDebugMessage("Failed to get IDXGIDevice interface\n");
        return false;
    }

    hr = m_d2dFactory->CreateDevice(dxgiDevice.Get(), m_d2dDevice.GetAddressOf());
    if (FAILED(hr)) {
        Platform::OutputDebugMessage("Failed to create D2D Device\n");
        return false;
    }

    // 6. Создаём D2D Device Context
    hr = m_d2dDevice->CreateDeviceContext(
        D2D1_DEVICE_CONTEXT_OPTIONS_NONE,
        m_d2dDeviceContext.GetAddressOf()
    );
    if (FAILED(hr)) {
        Platform::OutputDebugMessage("Failed to create D2D Device Context\n");
        return false;
    }

    // 7. Создаём текстовый формат по умолчанию
    hr = m_dwriteFactory->CreateTextFormat(
        L"Segoe UI",
        nullptr,
        DWRITE_FONT_WEIGHT_NORMAL,
        DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        16.0f,
        L"en-us",
        m_defaultTextFormat.GetAddressOf()
    );
    if (FAILED(hr)) {
        Platform::OutputDebugMessage("Failed to create default text format\n");
        return false;
    }

    // 8. Создаём solid brush для рисования
    hr = m_d2dDeviceContext->CreateSolidColorBrush(
        D2D1::ColorF(D2D1::ColorF::White),
        m_solidBrush.GetAddressOf()
    );
    if (FAILED(hr)) {
        Platform::OutputDebugMessage("Failed to create solid brush\n");
        return false;
    }

    Platform::OutputDebugMessage("Graphics2D initialized successfully\n");
    return true;
}

void Graphics2D::Shutdown() {
    ReleaseWindowSizeDependentResources();

    m_bitmaps.clear();
    m_solidBrush.Reset();
    m_defaultTextFormat.Reset();
    m_d2dDeviceContext.Reset();
    m_d2dDevice.Reset();
    m_d2dFactory.Reset();
    m_dwriteFactory.Reset();
    m_d3d11On12Device.Reset();
    m_d3d11DeviceContext.Reset();
    m_d3d11Device.Reset();
}

void Graphics2D::OnResize(UINT width, UINT height, ID3D12Resource** backBuffers, UINT backBufferCount) {
    ReleaseWindowSizeDependentResources();

    m_backBufferCount = backBufferCount;

    // Создаём wrapped resources для каждого back buffer
    for (UINT i = 0; i < backBufferCount; i++) {
        D3D11_RESOURCE_FLAGS d3d11Flags = { D3D11_BIND_RENDER_TARGET };

        HRESULT hr = m_d3d11On12Device->CreateWrappedResource(
            backBuffers[i],
            &d3d11Flags,
            D3D12_RESOURCE_STATE_RENDER_TARGET,
            D3D12_RESOURCE_STATE_PRESENT,
            IID_PPV_ARGS(&m_wrappedBackBuffers[i])
        );

        if (SUCCEEDED(hr)) {
            // Создаём D2D bitmap для рендеринга
            ComPtr<IDXGISurface> surface;
            hr = m_wrappedBackBuffers[i].As(&surface);

            if (SUCCEEDED(hr)) {
                D2D1_BITMAP_PROPERTIES1 bitmapProps = D2D1::BitmapProperties1(
                    D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
                    D2D1::PixelFormat(DXGI_FORMAT_R8G8B8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED)
                );

                hr = m_d2dDeviceContext->CreateBitmapFromDxgiSurface(
                    surface.Get(),
                    &bitmapProps,
                    m_d2dRenderTargets[i].GetAddressOf()
                );

                if (FAILED(hr)) {
                    Platform::OutputDebugMessage("Failed to create D2D render target bitmap\n");
                }
            }
        }
        else {
            Platform::OutputDebugMessage("Failed to create wrapped resource\n");
        }
    }
}

void Graphics2D::ReleaseWindowSizeDependentResources() {
    if (m_d3d11DeviceContext) {
        m_d3d11DeviceContext->OMSetRenderTargets(0, nullptr, nullptr);
    }
    if (m_d2dDeviceContext) {
        m_d2dDeviceContext->SetTarget(nullptr);
    }
    if (m_d3d11DeviceContext) {
        m_d3d11DeviceContext->Flush();
    }

    for (UINT i = 0; i < m_backBufferCount; i++) {
        m_d2dRenderTargets[i].Reset();
        m_wrappedBackBuffers[i].Reset();
    }
}

void Graphics2D::BeginDraw(UINT backBufferIndex) {
    if (backBufferIndex >= m_backBufferCount || !m_wrappedBackBuffers[backBufferIndex]) {
        return;
    }

    // Получаем wrapped resource для текущего back buffer
    ID3D11Resource* resources[] = { m_wrappedBackBuffers[backBufferIndex].Get() };
    m_d3d11On12Device->AcquireWrappedResources(resources, 1);

    // Устанавливаем render target
    m_d2dDeviceContext->SetTarget(m_d2dRenderTargets[backBufferIndex].Get());
    m_d2dDeviceContext->BeginDraw();

    // Устанавливаем transform (identity)
    m_d2dDeviceContext->SetTransform(D2D1::Matrix3x2F::Identity());
}

void Graphics2D::EndDraw(UINT backBufferIndex) {
    if (backBufferIndex >= m_backBufferCount || !m_wrappedBackBuffers[backBufferIndex]) {
        return;
    }

    HRESULT hr = m_d2dDeviceContext->EndDraw();

    if (hr == D2DERR_RECREATE_TARGET) {
        Platform::OutputDebugMessage("D2D: Need to recreate render target\n");
    }

    // Освобождаем wrapped resource
    ID3D11Resource* resources[] = { m_wrappedBackBuffers[backBufferIndex].Get() };
    m_d3d11On12Device->ReleaseWrappedResources(resources, 1);

    // Отправляем D3D11 команды
    m_d3d11DeviceContext->Flush();
}

D2D1_SIZE_F Graphics2D::GetRenderTargetSize() const {
    if (m_d2dDeviceContext) {
        return m_d2dDeviceContext->GetSize();
    }
    return D2D1::SizeF(0, 0);
}

// === Методы рисования ===

void Graphics2D::DrawLine(float x1, float y1, float x2, float y2,
    D2D1_COLOR_F color, float strokeWidth) {
    m_solidBrush->SetColor(color);
    m_d2dDeviceContext->DrawLine(
        D2D1::Point2F(x1, y1),
        D2D1::Point2F(x2, y2),
        m_solidBrush.Get(),
        strokeWidth
    );
}

void Graphics2D::DrawRectangle(float x, float y, float width, float height,
    D2D1_COLOR_F color, float strokeWidth) {
    m_solidBrush->SetColor(color);
    m_d2dDeviceContext->DrawRectangle(
        D2D1::RectF(x, y, x + width, y + height),
        m_solidBrush.Get(),
        strokeWidth
    );
}

void Graphics2D::FillRectangle(float x, float y, float width, float height,
    D2D1_COLOR_F color) {
    m_solidBrush->SetColor(color);
    m_d2dDeviceContext->FillRectangle(
        D2D1::RectF(x, y, x + width, y + height),
        m_solidBrush.Get()
    );
}

void Graphics2D::DrawRoundedRectangle(float x, float y, float width, float height,
    float radiusX, float radiusY,
    D2D1_COLOR_F color, float strokeWidth) {
    m_solidBrush->SetColor(color);
    D2D1_ROUNDED_RECT roundedRect = D2D1::RoundedRect(
        D2D1::RectF(x, y, x + width, y + height),
        radiusX, radiusY
    );
    m_d2dDeviceContext->DrawRoundedRectangle(roundedRect, m_solidBrush.Get(), strokeWidth);
}

void Graphics2D::FillRoundedRectangle(float x, float y, float width, float height,
    float radiusX, float radiusY,
    D2D1_COLOR_F color) {
    m_solidBrush->SetColor(color);
    D2D1_ROUNDED_RECT roundedRect = D2D1::RoundedRect(
        D2D1::RectF(x, y, x + width, y + height),
        radiusX, radiusY
    );
    m_d2dDeviceContext->FillRoundedRectangle(roundedRect, m_solidBrush.Get());
}

void Graphics2D::DrawEllipse(float centerX, float centerY, float radiusX, float radiusY,
    D2D1_COLOR_F color, float strokeWidth) {
    m_solidBrush->SetColor(color);
    m_d2dDeviceContext->DrawEllipse(
        D2D1::Ellipse(D2D1::Point2F(centerX, centerY), radiusX, radiusY),
        m_solidBrush.Get(),
        strokeWidth
    );
}

void Graphics2D::FillEllipse(float centerX, float centerY, float radiusX, float radiusY,
    D2D1_COLOR_F color) {
    m_solidBrush->SetColor(color);
    m_d2dDeviceContext->FillEllipse(
        D2D1::Ellipse(D2D1::Point2F(centerX, centerY), radiusX, radiusY),
        m_solidBrush.Get()
    );
}

void Graphics2D::DrawCircle(float centerX, float centerY, float radius,
    D2D1_COLOR_F color, float strokeWidth) {
    DrawEllipse(centerX, centerY, radius, radius, color, strokeWidth);
}

void Graphics2D::FillCircle(float centerX, float centerY, float radius,
    D2D1_COLOR_F color) {
    FillEllipse(centerX, centerY, radius, radius, color);
}

void Graphics2D::DrawText(const wchar_t* text, float x, float y, float width, float height,
    D2D1_COLOR_F color, float fontSize, const wchar_t* fontFamily) {
    // Создаём текстовый формат для указанного размера
    ComPtr<IDWriteTextFormat> textFormat;
    HRESULT hr = m_dwriteFactory->CreateTextFormat(
        fontFamily,
        nullptr,
        DWRITE_FONT_WEIGHT_NORMAL,
        DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        fontSize,
        L"en-us",
        textFormat.GetAddressOf()
    );

    if (SUCCEEDED(hr)) {
        m_solidBrush->SetColor(color);
        m_d2dDeviceContext->DrawText(
            text,
            static_cast<UINT32>(wcslen(text)),
            textFormat.Get(),
            D2D1::RectF(x, y, x + width, y + height),
            m_solidBrush.Get()
        );
    }
}

void Graphics2D::DrawTextCentered(const wchar_t* text, float x, float y, float width, float height,
    D2D1_COLOR_F color, float fontSize, const wchar_t* fontFamily) {
    ComPtr<IDWriteTextFormat> textFormat;
    HRESULT hr = m_dwriteFactory->CreateTextFormat(
        fontFamily,
        nullptr,
        DWRITE_FONT_WEIGHT_NORMAL,
        DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        fontSize,
        L"en-us",
        textFormat.GetAddressOf()
    );

    if (SUCCEEDED(hr)) {
        // Центрируем текст
        textFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        textFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);

        m_solidBrush->SetColor(color);
        m_d2dDeviceContext->DrawText(
            text,
            static_cast<UINT32>(wcslen(text)),
            textFormat.Get(),
            D2D1::RectF(x, y, x + width, y + height),
            m_solidBrush.Get()
        );
    }
}

bool Graphics2D::LoadBitmap(const wchar_t* filename, const String& name) {
    // Для загрузки изображений нужен WIC (Windows Imaging Component)
    // Полная реализация требует IWICImagingFactory
    // TODO: Implement with WIC
    Platform::OutputDebugMessage("LoadBitmap: WIC implementation not yet available\n");
    return false;
}

void Graphics2D::DrawBitmap(const String& name, float x, float y,
    float width, float height, float opacity) {
    auto it = m_bitmaps.find(name);
    if (it != m_bitmaps.end() && it->second) {
        D2D1_SIZE_F size = it->second->GetSize();
        float w = (width > 0) ? width : size.width;
        float h = (height > 0) ? height : size.height;

        m_d2dDeviceContext->DrawBitmap(
            it->second.Get(),
            D2D1::RectF(x, y, x + w, y + h),
            opacity
        );
    }
}

void Graphics2D::SetTransform(const D2D1_MATRIX_3X2_F& transform) {
    m_d2dDeviceContext->SetTransform(transform);
}

void Graphics2D::ResetTransform() {
    m_d2dDeviceContext->SetTransform(D2D1::Matrix3x2F::Identity());
}