#include "Win32InputBackend.h"

#include "Engine/Input/InputSystem.h"

#include <optional>

namespace
{
std::optional<Key> TranslateKey(WPARAM wParam, LPARAM lParam) noexcept
{
    const UINT virtualKey = static_cast<UINT>(wParam);

    if (virtualKey >= 'A' && virtualKey <= 'Z')
    {
        return static_cast<Key>(
            static_cast<UINT>(Key::A) + virtualKey - 'A');
    }

    if (virtualKey >= '0' && virtualKey <= '9')
    {
        return static_cast<Key>(
            static_cast<UINT>(Key::Digit0) + virtualKey - '0');
    }

    if (virtualKey >= VK_F1 && virtualKey <= VK_F12)
    {
        return static_cast<Key>(
            static_cast<UINT>(Key::F1) + virtualKey - VK_F1);
    }

    switch (virtualKey)
    {
    case VK_ESCAPE: return Key::Escape;
    case VK_SPACE: return Key::Space;
    case VK_RETURN: return Key::Enter;
    case VK_TAB: return Key::Tab;
    case VK_BACK: return Key::Backspace;
    case VK_INSERT: return Key::Insert;
    case VK_DELETE: return Key::Delete;
    case VK_HOME: return Key::Home;
    case VK_END: return Key::End;
    case VK_PRIOR: return Key::PageUp;
    case VK_NEXT: return Key::PageDown;
    case VK_UP: return Key::ArrowUp;
    case VK_DOWN: return Key::ArrowDown;
    case VK_LEFT: return Key::ArrowLeft;
    case VK_RIGHT: return Key::ArrowRight;
    case VK_CAPITAL: return Key::CapsLock;
    case VK_LSHIFT: return Key::LeftShift;
    case VK_RSHIFT: return Key::RightShift;
    case VK_LCONTROL: return Key::LeftControl;
    case VK_RCONTROL: return Key::RightControl;
    case VK_LMENU: return Key::LeftAlt;
    case VK_RMENU: return Key::RightAlt;
    case VK_SHIFT:
    {
        const UINT scanCode =
            static_cast<UINT>((lParam >> 16) & 0xFF);
        const UINT translated =
            MapVirtualKeyW(scanCode, MAPVK_VSC_TO_VK_EX);
        return translated == VK_RSHIFT
            ? Key::RightShift
            : Key::LeftShift;
    }
    case VK_CONTROL:
        return (lParam & (1 << 24)) != 0
            ? Key::RightControl
            : Key::LeftControl;
    case VK_MENU:
        return (lParam & (1 << 24)) != 0
            ? Key::RightAlt
            : Key::LeftAlt;
    default:
        return std::nullopt;
    }
}

Vector2 MousePositionFromLParam(LPARAM lParam) noexcept
{
    return {
        static_cast<float>(static_cast<short>(LOWORD(lParam))),
        static_cast<float>(static_cast<short>(HIWORD(lParam)))
    };
}
}

Win32InputBackend::~Win32InputBackend()
{
    Shutdown();
}

bool Win32InputBackend::Initialize(
    Window& window,
    InputSystem& inputSystem) noexcept
{
    if (window_ != nullptr || window.GetNativeHandle() == nullptr)
        return false;

    window_ = &window;
    inputSystem_ = &inputSystem;
    inputSystem_->Reset();
    inputSystem_->BeginFrame();
    inputSystem_->SubmitFocus(
        GetFocus() == window.GetNativeHandle());
    inputSystem_->EndFrame();

    window_->SetMessageObserver(
        &Win32InputBackend::ObserveWindowMessage,
        this);

    return true;
}

void Win32InputBackend::Shutdown() noexcept
{
    if (window_ != nullptr)
    {
        window_->ClearMessageObserver();
        window_ = nullptr;
    }

    inputSystem_ = nullptr;
}

void Win32InputBackend::ObserveWindowMessage(
    void* userData,
    HWND hwnd,
    UINT message,
    WPARAM wParam,
    LPARAM lParam)
{
    auto* self = static_cast<Win32InputBackend*>(userData);
    if (self != nullptr && self->inputSystem_ != nullptr)
        self->ProcessWindowMessage(hwnd, message, wParam, lParam);
}

void Win32InputBackend::ProcessWindowMessage(
    HWND,
    UINT message,
    WPARAM wParam,
    LPARAM lParam) noexcept
{
    switch (message)
    {
    case WM_SETFOCUS:
        inputSystem_->SubmitFocus(true);
        break;
    case WM_KILLFOCUS:
        inputSystem_->SubmitFocus(false);
        break;

    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
        if (const auto key = TranslateKey(wParam, lParam))
            inputSystem_->SubmitKey(*key, true);
        break;
    case WM_KEYUP:
    case WM_SYSKEYUP:
        if (const auto key = TranslateKey(wParam, lParam))
            inputSystem_->SubmitKey(*key, false);
        break;

    case WM_LBUTTONDOWN:
        inputSystem_->SubmitMouseButton(MouseButton::Left, true);
        break;
    case WM_LBUTTONUP:
        inputSystem_->SubmitMouseButton(MouseButton::Left, false);
        break;
    case WM_RBUTTONDOWN:
        inputSystem_->SubmitMouseButton(MouseButton::Right, true);
        break;
    case WM_RBUTTONUP:
        inputSystem_->SubmitMouseButton(MouseButton::Right, false);
        break;
    case WM_MBUTTONDOWN:
        inputSystem_->SubmitMouseButton(MouseButton::Middle, true);
        break;
    case WM_MBUTTONUP:
        inputSystem_->SubmitMouseButton(MouseButton::Middle, false);
        break;
    case WM_XBUTTONDOWN:
        inputSystem_->SubmitMouseButton(
            GET_XBUTTON_WPARAM(wParam) == XBUTTON1
                ? MouseButton::X1
                : MouseButton::X2,
            true);
        break;
    case WM_XBUTTONUP:
        inputSystem_->SubmitMouseButton(
            GET_XBUTTON_WPARAM(wParam) == XBUTTON1
                ? MouseButton::X1
                : MouseButton::X2,
            false);
        break;

    case WM_MOUSEMOVE:
        inputSystem_->SubmitMouseMove(
            MousePositionFromLParam(lParam));
        break;
    case WM_MOUSEWHEEL:
        inputSystem_->SubmitMouseWheel(
            static_cast<float>(GET_WHEEL_DELTA_WPARAM(wParam)) /
            static_cast<float>(WHEEL_DELTA));
        break;
    }
}
