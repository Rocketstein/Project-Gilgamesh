#pragma once
#include "Engine/Platform/Windows/Window.h"

struct RendererDesc
{
	HWND outputWindow = nullptr;
	Extent2D extent{};
	bool vsync = true;
};

class Renderer
{
public:
	bool Initialize(RendererDesc desc);
	bool Resize(Extent2D extent);


private:
	// D3D11 resources

};