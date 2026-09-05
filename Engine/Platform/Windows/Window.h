#pragma once
#include <Windows.h>

#include "Engine/Core/Extent2D.h"

struct WindowMessageResult
{
	// handled tells Window whether to return result instead of calling
	// DefWindowProc for a message it does not own itself.
	bool handled = false;
	LRESULT result = 0;
};

// A generic Win32 message adapter. userData lets a static callback recover
// its owning object without Window knowing that object's concrete type.
using WindowMessageHandler = WindowMessageResult(*)(
	void* userData,
	HWND hwnd,
	UINT message,
	WPARAM wParam,
	LPARAM lParam);

// A non-consuming observer sees messages without affecting normal dispatch.
// This is kept separate from WindowMessageHandler so physical input state is
// recorded even when an integration such as ImGui handles the same message.
using WindowMessageObserver = void(*)(
	void* userData,
	HWND hwnd,
	UINT message,
	WPARAM wParam,
	LPARAM lParam);

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

	void SetMessageHandler(
		WindowMessageHandler handler,
		void* userData = nullptr) noexcept;

	void ClearMessageHandler() noexcept;

	void SetMessageObserver(
		WindowMessageObserver observer,
		void* userData = nullptr) noexcept;

	void ClearMessageObserver() noexcept;

private:
	static LRESULT CALLBACK WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
	LRESULT HandleMessage(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

	// One optional external consumer is sufficient for now. This can evolve
	// into a subscription list when a second real consumer appears.
	WindowMessageHandler messageHandler_ = nullptr;
	void* messageHandlerUserData_		 = nullptr;
	WindowMessageObserver messageObserver_ = nullptr;
	void* messageObserverUserData_ = nullptr;
	HWND hwnd_          = nullptr;
	bool quitRequested_ = false;
	bool isMinimized_   = false;
	bool pendingResize_	= false;
};
