#include "Renderer.h"
#include "D3D11/D3D11Device.h"

bool Renderer::Initialize(RendererDesc desc)
{
    if (desc.outputWindow == nullptr ||
        desc.extent.width == 0 ||
        desc.extent.height == 0)
    {
        return false;
    }

#ifdef _DEBUG
	constexpr bool requestDebugLayer = true;
#else
	constexpr bool requestDebugLayer = false;
#endif
	HRESULT result = device_.Initialize(requestDebugLayer);
	if (FAILED(result)) return false;

	result = surface_.Initialize(
		device_.GetDevice(),
		desc.outputWindow,
		desc.extent,
		desc.vsync);

	if (FAILED(result)) return false;

	return true;
}

bool Renderer::Resize(Extent2D extent)
{
	// Handle resizing logic here, not WndProc.
	if (extent.width == 0 || extent.height == 0) return true;
	HRESULT result = surface_.Resize(device_.GetDevice(), device_.GetDeviceContext(), extent);
	if (FAILED(result)) return false;
	return true;
}

RenderResult Renderer::Render(const Color4& clearColor)
{
    if (!device_.IsInitialized() ||
        !surface_.IsInitialized())
    {
        return RenderResult::Failed;
    }

    surface_.BeginFrame(
        device_.GetDeviceContext(),
        clearColor);

    // Graphics pipeline draw calls will eventually go here.

    const HRESULT result = surface_.Present();

    if (SUCCEEDED(result))
        return RenderResult::Ok;

    if (result == DXGI_ERROR_DEVICE_REMOVED ||
        result == DXGI_ERROR_DEVICE_RESET)
    {
        return RenderResult::DeviceLost;
    }

    return RenderResult::Failed;
}