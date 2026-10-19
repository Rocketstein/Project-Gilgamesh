#include "ViewportClearPass.h"

#include "Engine/Core/Logging/Logger.h"
#include "Engine/Render/RenderGraph/RenderGraph.h"

ViewportClearPass::ViewportClearPass(Color4 color) noexcept
	: clearColor_(color)
{
}

bool ViewportClearPass::Initialize(ID3D11Device* device, ShaderManager&)
{
	// Keep invalid renderer services from initializing a configured pipeline.
	if (!device)
	{
		GILGAMESH_LOG(Renderer, Error, "Viewport clear pass requires a device");
		return false;
	}
	return true;
}

bool ViewportClearPass::AddToGraph(
	RenderGraph& graph,
	const RenderPassContext& context) const
{
	// Reject invalid targets before changing the graph's declarations.
	if (!graph.IsValid(context.color) || !graph.IsValid(context.depth))
	{
		GILGAMESH_LOG(Renderer, Error,
			"Cannot clear a viewport without valid graph color and depth targets");
		return false;
	}

	// Subsequent passes load these initialized attachments in registration order.
	graph.AddPass(
		"ViewportClear",
		[color = context.color, depth = context.depth, clearColor = clearColor_](
			GraphPassBuilder& builder)
		{
			builder.SetColorTarget(0, color, RenderGraphLoadOp::Clear, clearColor);
			builder.SetDepthTarget(
				depth,
				RenderGraphDepthAccess::ReadWrite,
				RenderGraphLoadOp::Clear,
				1.0f);
		},
		[](ID3D11DeviceContext&) {});
	return true;
}
