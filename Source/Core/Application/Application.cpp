#include "Application.h"


// Static instance
Application* Application::s_instance = nullptr;

Application::Application(const ApplicationConfig& config)
    : m_config(config) {
    ASSERT(s_instance == nullptr, "Application instance already exists!");
    s_instance = this;

    if (!CreateAppWindow()) {
		throw std::runtime_error("Window init failed");
    }

    SetupEventCallbacks();

	m_graphics = std::make_unique<Graphics>(m_window.get());
    if (!m_graphics) {
        throw std::runtime_error("Graphics initialization failed");
	}

    // Initialize timer
    m_timer.Reset();
    m_timer.Start();

    Platform::OutputDebugMessage("Application initialized successfully\n");

    m_initialized = true;
}

Application::~Application() {
    if (m_initialized) {
        Shutdown();
    }
    s_instance = nullptr;
}

int32 Application::Run() {
    if (!m_initialized) {
        Platform::OutputDebugMessage("Application not initialized\n");
        return -1;
    }

    Platform::OutputDebugMessage("Starting main loop\n");

    // Show window
    m_window->Show();

    // Main loop
    MainLoop();

    Platform::OutputDebugMessage("Main loop ended\n");
    return 0;
}

void Application::Shutdown() {
    if (!m_initialized) {
        return;
    }

    Platform::OutputDebugMessage("Shutting down application\n");

    // Call derived class shutdown
    OnShutdown();

    // Cleanup window
    if (m_window) {
        m_window->Destroy();
        m_window.reset();
    }

    m_initialized = false;
    Platform::OutputDebugMessage("Application shutdown complete\n");
}

bool Application::CreateAppWindow() {
    m_window = Window::Create();
    if (!m_window) {
        return false;
    }

    return m_window->Create(m_config.windowDesc);
}

void Application::SetupEventCallbacks() {
    m_window->SetWindowResizeEventCallback(
        [this](const WindowResizeEvent& event) {
            HandleWindowResize(event);
        }
    );

    m_window->SetWindowCloseEventCallback(
        [this]() {
            HandleWindowClose();
        }
    );

    m_window->SetKeyEventCallback(
        [this](const KeyEvent& event) {
            HandleKeyEvent(event);
        }
    );

    m_window->SetMouseButtonEventCallback(
        [this](const MouseButtonEvent& event) {
            HandleMouseButtonEvent(event);
        }
    );

    m_window->SetMouseMoveEventCallback(
        [this](const MouseMoveEvent& event) {
            HandleMouseMoveEvent(event);
        }
    );

    m_window->SetMouseWheelEventCallback(
        [this](const MouseWheelEvent& event) {
            HandleMouseWheelEvent(event);
        }
    );

    m_window->SetWindowPauseEventCallback(
        [this](bool paused) {
            SetPaused(paused);
            if (paused) {
                m_timer.Stop();
            } else {
                m_timer.Start();
            }
        }
    );
}

void Application::MainLoop() {
    Platform::OutputDebugMessage("Entering main loop\n");

    while (!m_shouldExit && !m_window->ShouldClose()) {
        // Poll window events
        m_window->PollEvents();

        // Double-check if window should close after polling events
        if (m_window->ShouldClose()) {
            Platform::OutputDebugMessage("Window should close detected in main loop\n");
            break;
        }

        // Update timer
        m_timer.Tick();

        if (!m_isAppPaused) {
			// Update and render
			Update();
			Render();
        } else {
            Sleep(100);
		}
    }

    Platform::OutputDebugMessage("Exiting main loop - shouldExit: " +
                                std::to_string(m_shouldExit) +
                                ", windowShouldClose: " +
                                std::to_string(m_window->ShouldClose()) + "\n");
}

void Application::Update() {
    float32 deltaTime = m_timer.GetDeltaTime();

    // Call derived class update
    OnUpdate(deltaTime);

	// Update graphics
	m_graphics->Update(deltaTime);
}

void Application::Render() {
    // Call derived class render
    OnRender();

	// Render graphics
	m_graphics->DrawFrame();
}

void Application::HandleWindowResize(const WindowResizeEvent& event) {
    if (m_graphics) {
        m_graphics->OnResize();
	}

    OnWindowResize(event.width, event.height);
}

void Application::HandleWindowClose() {
    Platform::OutputDebugMessage("Window close requested\n");
    m_shouldExit = true;
}

void Application::HandleKeyEvent(const KeyEvent& event) {
    if (event.pressed && m_graphics) {
        m_graphics->OnKeyDown(event.key);
    }
    OnKeyEvent(event);
}

void Application::HandleMouseButtonEvent(const MouseButtonEvent& event) {
    if (event.pressed) {
        m_graphics->OnMouseDown(event.button, event.x, event.y);
    } else {
        m_graphics->OnMouseUp(event.button, event.x, event.y);
    }
    OnMouseButtonEvent(event);
}

void Application::HandleMouseMoveEvent(const MouseMoveEvent& event) {
    m_graphics->OnMouseMove(event.x, event.y);
    OnMouseMoveEvent(event);
}

void Application::HandleMouseWheelEvent(const MouseWheelEvent& event) {
    OnMouseWheelEvent(event);
}