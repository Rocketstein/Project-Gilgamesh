#include "Renderer.h"

#include <stdexcept>
#include <utility>

#include "Engine/Core/Logging/Logger.h"
#include "Engine/Render/RenderGraph/RenderGraph.h"

namespace {
    const Color4 clearColor = Color4{ 0.1f, 0.12f, 0.16f };
} // Anonymous namespace

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

	// Initialize renderer-owned mesh resources after the D3D11 device.
	if (!meshManager_.Initialize(device_.GetDevice())) return false;

	// Initialize the mesh pass after its shaders and device.
	if (!vertexColorMeshPass_.Initialize(
		device_.GetDevice(),
		shaderManager_))
	{
		return false;
	}

    rendererDesc_ = desc;

	return true;
}

bool Renderer::Resize(Extent2D extent)
{
	// Handle resizing logic here, not WndProc.
	if (extent.width == 0 || extent.height == 0) return true;
	HRESULT result = surface_.Resize(device_.GetDevice(), device_.GetDeviceContext(), extent);
	if (FAILED(result)) return false;

    rendererDesc_.extent = extent;
	return true;
}

bool Renderer::BeginFrame()
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

void Renderer::BindOutput() noexcept
{
    surface_.Bind(device_.GetDeviceContext());
}

bool Renderer::AddScenePasses(
	RenderGraph& graph,
	RenderGraphTextureHandle color,
	RenderGraphTextureHandle depth,
	const RenderFrameData& frame,
	const RenderView& view) const
{
	// Reject invalid submissions before adding any declarations to the graph.
	if (!vertexColorMeshPass_.IsInitialized() ||
		!graph.IsValid(color) || !graph.IsValid(depth))
	{
		GILGAMESH_LOG(Renderer, Error,
			"Cannot add a mesh pass without initialized resources and valid graph targets");
		return false;
	}
	auto commands = vertexColorMeshPass_.PrepareCommands(frame, view, meshManager_);
	if (!commands) return false;

	// The graph owns the prepared values; extraction data may be released now.
	graph.AddPass(
		"VertexColorMeshes",
		[color, depth](GraphPassBuilder& builder)
		{
			builder.SetColorTarget(0, color, RenderGraphLoadOp::Clear, clearColor);
			builder.SetDepthTarget(
				depth,
				RenderGraphDepthAccess::ReadWrite,
				RenderGraphLoadOp::Clear,
				1.0f);
		},
		[this, commands = std::move(*commands)](ID3D11DeviceContext& context)
		{
			// RenderGraph callbacks propagate failure to the application's render boundary.
			if (!vertexColorMeshPass_.ExecuteCommands(&context, commands, meshManager_))
			{
				throw std::runtime_error("Failed to execute vertex-color mesh commands");
			}
		});
	return true;
}

RenderResult Renderer::EndFrame()
{
    const HRESULT result = surface_.Present();

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
