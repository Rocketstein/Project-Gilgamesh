#pragma once

#include "Engine/Core/Extent2D.h"
#include "D3D11/D3D11Device.h"
#include "D3D11/PresentationSurface.h"

struct RendererDesc
{
	HWND outputWindow = nullptr;
	Extent2D extent = {};
	bool vsync = true;
};

enum class RenderResult
{
	Ok,
	DeviceLost,
	Failed
};

class Renderer
{
public:
	bool Initialize(RendererDesc desc);
	bool Resize(Extent2D extent);
	RenderResult Render(const Color4& clearColor);

private:
	RendererDesc rendererDesc_;

	// D3D11 resources
	D3D11Device device_;
	D3D11PresentationSurface surface_;
};