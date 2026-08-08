#pragma once

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
	Occluded,
	DeviceLost,
	Failed
};

class Renderer
{
public:
	bool Initialize(RendererDesc desc);
	bool Resize(Extent2D extent);
	RenderResult Render(const Color4& clearColor);

	[[nodiscard]]
	bool IsOccluded() const { return surface_.PresentTest() == DXGI_STATUS_OCCLUDED; }

	[[nodiscard]]
	ID3D11Device* GetDevice() const { return device_.GetDevice(); }

	[[nodiscard]]
	ID3D11DeviceContext* GetDeviceContext() const { return device_.GetDeviceContext(); }

private:
	RendererDesc rendererDesc_;

	// D3D11 resources
	D3D11Device device_;
	D3D11PresentationSurface surface_;
};