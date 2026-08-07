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
	bool Create();
	bool PumpMessages();
	bool IsMinimized() const;
	bool ConsumePendingResize();
	HWND NativeHandle() const;
	Extent2D ClientExtent() const;

};