#pragma once
#include <Windows.h>
#include <cstdint>

struct Extent2D
{
	std::uint32_t width = 0;
	std::uint32_t height = 0;
};

class Window
{
public:
	Window() = default;
	~Window();

	// Explicit ownership semantics: Window is not copyable or assignable.
	Window(const Window&) = delete;
	Window& operator=(const Window&) = delete; 

	bool Create();
	bool PumpMessages();
	bool IsMinimized() const;
	bool ConsumePendingResize();
	HWND NativeHandle() const;
	Extent2D ClientExtent() const;

private:
	static LRESULT CALLBACK WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
	LRESULT HandleMessage(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

	HWND hwnd_          = nullptr;
	bool quitRequested_ = false;
	bool isMinimized_   = false;
	bool pendingResize_		= false;
};