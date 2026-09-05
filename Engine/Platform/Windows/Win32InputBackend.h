#pragma once

#include "Engine/Platform/Windows/Window.h"

class InputSystem;

// Translates Win32 messages into platform-neutral InputSystem submissions.
// It observes messages but never consumes them, so ImGui and the default
// window procedure continue to receive their normal message flow.
class Win32InputBackend
{
public:
    Win32InputBackend() = default;
    ~Win32InputBackend();

    Win32InputBackend(const Win32InputBackend&) = delete;
    Win32InputBackend& operator=(const Win32InputBackend&) = delete;

    bool Initialize(Window& window, InputSystem& inputSystem) noexcept;
    void Shutdown() noexcept;

private:
    static void ObserveWindowMessage(
        void* userData,
        HWND hwnd,
        UINT message,
        WPARAM wParam,
        LPARAM lParam);

    void ProcessWindowMessage(
        HWND hwnd,
        UINT message,
        WPARAM wParam,
        LPARAM lParam) noexcept;

    Window* window_ = nullptr;
    InputSystem* inputSystem_ = nullptr;
};
