#include "Window.h"

Window::~Window()
{
	if (hwnd_ != nullptr)
		DestroyWindow(hwnd_);
}

LRESULT CALLBACK Window::WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	// Recover the instance handed to CreateWindowEx as its last argument.
	if (uMsg == WM_NCCREATE)
	{
		auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
		auto* self   = static_cast<Window*>(create->lpCreateParams);
		self->hwnd_  = hwnd;
		SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
	}

	if (auto* self =
		reinterpret_cast<Window*>(
			GetWindowLongPtrW(hwnd, GWLP_USERDATA)))
	{
		const LRESULT result =
			self->HandleMessage(hwnd, uMsg, wParam, lParam);

		if (uMsg == WM_NCDESTROY)
		{
			SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
			self->hwnd_ = nullptr;
		}

		return result;
	}

	return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

LRESULT Window::HandleMessage(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	// Let the registered integration observe every message first, but defer its
	// return value until Window has handled the lifecycle messages it owns.
	WindowMessageResult externalResult{};
	if (messageHandler_ != nullptr)
	{
		externalResult = messageHandler_(
			messageHandlerUserData_,
			hwnd,
			uMsg,
			wParam,
			lParam);
	}

	switch (uMsg)
	{
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	case WM_SIZE:
		if (wParam == SIZE_MINIMIZED)
		{
			// Window is minimized.
			isMinimized_ = true;
		}
		else if (wParam == SIZE_MAXIMIZED)
		{
			// Window is maximized.
			isMinimized_   = false;
			pendingResize_ = true;
		}
		else if (wParam == SIZE_RESTORED)
		{
			// Window is restored to normal size.
			isMinimized_   = false;
			pendingResize_ = true;
		}
		return 0;
	}

	if (externalResult.handled)
	{
		// The external consumer handled a message that Window did not own.
		return externalResult.result;
	}

	// Neither Window nor the external consumer handled this message.
	return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

bool Window::Create()
{
	if (hwnd_ != nullptr) return false;

	const wchar_t* kClassName = L"GilgameshWindowClass";
	HINSTANCE hInstance = GetModuleHandle(nullptr);

	WNDCLASSEX wc = {};
	wc.cbSize = sizeof(WNDCLASSEX);
	wc.style = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc = WndProc;
	wc.hInstance = hInstance;
	wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wc.lpszClassName = kClassName;

	const ATOM windowClass = RegisterClassExW(&wc);
	if (windowClass == 0 &&
		GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
	{
		return false;
	}

	RECT rc = { 0, 0, 1280 ,720 };
	if (!AdjustWindowRect(
		&rc,
		WS_OVERLAPPEDWINDOW,
		FALSE))
	{
		return false;
	}

	// WndProc picks up the trailing this on WM_NCCREATE.
	HWND hwnd = CreateWindowEx(0, kClassName, L"Project Gilgamesh", WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT, rc.right - rc.left, rc.bottom - rc.top,
		nullptr, nullptr, hInstance, this);
	if (hwnd == nullptr) return false;

	ShowWindow(hwnd, SW_SHOW);

	return true;
}

bool Window::PumpMessages()
{
	// Game Loop Usage
	// while (window.PumpMessages())
	// {
	//	 // Update simulation.
	//	 // Build render data.
	//	 // Render and present.
	// }

	if (quitRequested_) return false;

	MSG message{};

	while (PeekMessageW(
		&message,
		nullptr,
		0,
		0,
		PM_REMOVE))
	{
		if (message.message == WM_QUIT)
		{
			quitRequested_ = true;
			return false;
		}

		TranslateMessage(&message);
		DispatchMessageW(&message);
	}

	return true;
}

bool Window::IsMinimized() const
{
	return isMinimized_;
}

bool Window::ConsumePendingResize()
{
	// Consume the flag so a resize is reported to the caller exactly once.
	const bool resized = pendingResize_;
	pendingResize_ = false;
	return resized;
}

HWND Window::GetNativeHandle() const
{
	return hwnd_;
}

Extent2D Window::GetClientExtent() const
{
	RECT rc{};
	if (hwnd_ == nullptr || !GetClientRect(hwnd_, &rc)) return {};

	return Extent2D{
		static_cast<std::uint32_t>(rc.right - rc.left),
		static_cast<std::uint32_t>(rc.bottom - rc.top)
	};
}

void Window::SetMessageHandler(
	WindowMessageHandler handler,
	void* userData) noexcept
{
	// Store the function and its opaque owner as a pair. Window never needs to
	// know that the current owner is ImGuiIntegration.
	messageHandler_ = handler;
	messageHandlerUserData_ = userData;
}

void Window::ClearMessageHandler() noexcept
{
	// Clear both values together so no stale owner can be called later.
	messageHandler_ = nullptr;
	messageHandlerUserData_ = nullptr;
}
