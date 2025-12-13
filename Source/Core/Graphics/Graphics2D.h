#pragma once

#include <WindowsPlatform.h>

// Forward declarations
struct ID3D12Device;
struct ID3D12CommandQueue;
struct ID3D12Resource;
struct IDXGISwapChain;

class Graphics2D {
public:
    Graphics2D() = default;
    ~Graphics2D();

    // Запрет копирования и перемещения
    Graphics2D(const Graphics2D&) = delete;
    Graphics2D& operator=(const Graphics2D&) = delete;
    Graphics2D(Graphics2D&&) = delete;
    Graphics2D& operator=(Graphics2D&&) = delete;

    // Инициализация D2D системы
    bool Initialize(
        ID3D12Device* d3d12Device,
        ID3D12CommandQueue* commandQueue,
        IDXGISwapChain* swapChain,
        UINT backBufferCount,
        DXGI_FORMAT backBufferFormat
    );

    // Освобождение ресурсов
    void Shutdown();

    // Подготовка к рендерингу (вызывать после D3D12 рендеринга)
    void BeginDraw(UINT backBufferIndex);

    // Завершение рендеринга
    void EndDraw(UINT backBufferIndex);

    // Обработка изменения размера окна
    void OnResize(UINT width, UINT height, ID3D12Resource** backBuffers, UINT backBufferCount);

    // === Методы рисования ===

    // Линии
    void DrawLine(float x1, float y1, float x2, float y2,
        D2D1_COLOR_F color, float strokeWidth = 1.0f);

    // Прямоугольники
    void DrawRectangle(float x, float y, float width, float height,
        D2D1_COLOR_F color, float strokeWidth = 1.0f);

    void FillRectangle(float x, float y, float width, float height,
        D2D1_COLOR_F color);

    void DrawRoundedRectangle(float x, float y, float width, float height,
        float radiusX, float radiusY,
        D2D1_COLOR_F color, float strokeWidth = 1.0f);

    void FillRoundedRectangle(float x, float y, float width, float height,
        float radiusX, float radiusY,
        D2D1_COLOR_F color);

    // Эллипсы и круги
    void DrawEllipse(float centerX, float centerY, float radiusX, float radiusY,
        D2D1_COLOR_F color, float strokeWidth = 1.0f);

    void FillEllipse(float centerX, float centerY, float radiusX, float radiusY,
        D2D1_COLOR_F color);

    void DrawCircle(float centerX, float centerY, float radius,
        D2D1_COLOR_F color, float strokeWidth = 1.0f);

    void FillCircle(float centerX, float centerY, float radius,
        D2D1_COLOR_F color);

    // Текст
    void DrawText(const wchar_t* text, float x, float y, float width, float height,
        D2D1_COLOR_F color, float fontSize = 16.0f,
        const wchar_t* fontFamily = L"Segoe UI");

    void DrawTextCentered(const wchar_t* text, float x, float y, float width, float height,
        D2D1_COLOR_F color, float fontSize = 16.0f,
        const wchar_t* fontFamily = L"Segoe UI");

    // Изображения (Bitmap)
    bool LoadBitmap(const wchar_t* filename, const String& name);
    void DrawBitmap(const String& name, float x, float y,
        float width = 0, float height = 0, float opacity = 1.0f);

    // Трансформации
    void SetTransform(const D2D1_MATRIX_3X2_F& transform);
    void ResetTransform();

    // Получить размеры render target
    D2D1_SIZE_F GetRenderTargetSize() const;

    // Прямой доступ к D2D контексту для расширенного использования
    ID2D1DeviceContext2* GetDeviceContext() { return m_d2dDeviceContext.Get(); }
    IDWriteFactory3* GetDWriteFactory() { return m_dwriteFactory.Get(); }

private:
    // D3D11on12 interop
    ComPtr<ID3D11Device> m_d3d11Device;
    ComPtr<ID3D11DeviceContext> m_d3d11DeviceContext;
    ComPtr<ID3D11On12Device> m_d3d11On12Device;

    // Direct2D
    ComPtr<ID2D1Factory3> m_d2dFactory;
    ComPtr<ID2D1Device2> m_d2dDevice;
    ComPtr<ID2D1DeviceContext2> m_d2dDeviceContext;

    // DirectWrite
    ComPtr<IDWriteFactory3> m_dwriteFactory;
    ComPtr<IDWriteTextFormat> m_defaultTextFormat;

    // Wrapped back buffers
    static const UINT MAX_BACK_BUFFERS = 3;
    ComPtr<ID3D11Resource> m_wrappedBackBuffers[MAX_BACK_BUFFERS];
    ComPtr<ID2D1Bitmap1> m_d2dRenderTargets[MAX_BACK_BUFFERS];
    UINT m_backBufferCount = 0;

    // Brushes (кэшированные для производительности)
    ComPtr<ID2D1SolidColorBrush> m_solidBrush;

    // Bitmaps
    HashMap<String, ComPtr<ID2D1Bitmap1>> m_bitmaps;

    // Helpers
    void CreateDeviceResources();
    void CreateWindowSizeDependentResources(ID3D12Resource** backBuffers, UINT count);
    void ReleaseWindowSizeDependentResources();
};