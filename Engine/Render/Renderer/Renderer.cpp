#include "Renderer.h"

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

    // Initialize Device
	HRESULT result = device_.Initialize(requestDebugLayer);
	if (FAILED(result)) return false;

    // Initialize Swap Chain
	result = surface_.Initialize(
		device_.GetDevice(),
		desc.outputWindow,
		desc.extent,
		desc.vsync);
	if (FAILED(result)) return false;

    // Initialize Shaders
    if (!shaderManager_.Initialize(device_.GetDevice(), desc.shaderDirectory)) return false;

    rendererDesc_ = desc;

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

bool Renderer::BeginFrame(const Color4& clearColor)
{
    if (!device_.IsInitialized() ||
        !surface_.IsInitialized())
    {
        return false;
    }

    surface_.BeginFrame(
        device_.GetDeviceContext(),
        clearColor
    );

    return true;
}

RenderResult Renderer::EndFrame()
{
    const HRESULT result = surface_.Present();

    // Graphics pipeline draw calls will eventually go here.

    if (result == DXGI_STATUS_OCCLUDED)
        return RenderResult::Occluded;

    if (SUCCEEDED(result))
        return RenderResult::Ok;

    if (result == DXGI_ERROR_DEVICE_REMOVED ||
        result == DXGI_ERROR_DEVICE_RESET)
    {
        return RenderResult::DeviceLost;
    }

    return RenderResult::Failed;
}