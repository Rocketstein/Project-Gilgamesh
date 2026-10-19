#pragma once

#include "Engine/Core/Color.h"
#include "IRenderPass.h"

// Starts a viewport submission with initialized color and depth attachments.
class ViewportClearPass final : public IRenderPass
{
public:
	explicit ViewportClearPass(Color4 color = { 0.1f, 0.12f, 0.16f }) noexcept;

	// This attachment-only pass has no GPU resources to allocate.
	[[nodiscard]]
	bool Initialize(ID3D11Device* device, ShaderManager& shaders) override;

	// Declares the clears; RenderGraph performs them before the empty callback.
	[[nodiscard]]
	bool AddToGraph(RenderGraph& graph, const RenderPassContext& context) const override;

private:
	Color4 clearColor_; // Background color applied before every viewport pass.
};
