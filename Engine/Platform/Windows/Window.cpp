#include "Window.h"

namespace
{
	LRESULT CALLBACK WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
	{
		switch (uMsg)
		{
		case WM_DESTROY:
			PostQuitMessage(0);
			return 0;
		default:
			return DefWindowProc(hwnd, uMsg, wParam, lParam);
		}
	}
}

bool Window::Create()
{
	const wchar_t* kClassName = L"GilgameshWindowClass";
	HINSTANCE hInstance = GetModuleHandle(nullptr);

	WNDCLASSEX wc = {};
	wc.cbSize = sizeof(WNDCLASSEX);
	wc.style = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc = WndProc;
	wc.hInstance = hInstance;
	wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wc.lpszClassName = kClassName;
	RegisterClassEx(&wc);

	RECT rc = { 0, 0, 1280 ,720 };
	AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);

	HWND hwnd = CreateWindowEx(0, kClassName, L"Project Gilgamesh", WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT, rc.right - rc.left, rc.bottom - rc.top,
		nullptr, nullptr, hInstance, nullptr);
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
	return false;
}

bool Window::ConsumePendingResize()
{
	return false;
}

HWND Window::NativeHandle() const
{

}

Extent2D Window::ClientExtent() const
{
	
}