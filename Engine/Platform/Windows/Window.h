#pragma once
#include <Windows.h>

#include "Engine/Core/Extent2D.h"

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
	HWND GetNativeHandle() const;
	Extent2D GetClientExtent() const;

private:
	static LRESULT CALLBACK WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
	LRESULT HandleMessage(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

	HWND hwnd_          = nullptr;
	bool quitRequested_ = false;
	bool isMinimized_   = false;
	bool pendingResize_	= false;
};