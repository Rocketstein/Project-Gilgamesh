#pragma once

#include <d3d11.h>

#include "Engine/Render/RenderGraph/Resource/RenderGraphTexture.h"

class MeshManager;
class RenderGraph;
class ShaderManager;
struct RenderFrameData;
struct RenderView;

// Borrows one viewport submission while passes prepare graph-owned commands.
struct RenderPassContext
{
	RenderGraphTextureHandle color; // Shared output for the ordered pass list.
	RenderGraphTextureHandle depth; // Shared depth for passes that require it.
	const RenderFrameData& frame; // Copied scene extraction; never captured by reference.
	const RenderView& view; // Camera and extent used when preparing this submission.
	const MeshManager& meshes; // Must outlive every graph callback that resolves meshes.
};

// Owns pass resources and declares their work without owning viewport targets.
class IRenderPass
{
public:
	virtual ~IRenderPass() = default;

	IRenderPass(const IRenderPass&) = delete;
	IRenderPass& operator=(const IRenderPass&) = delete;
	IRenderPass(IRenderPass&&) = delete;
	IRenderPass& operator=(IRenderPass&&) = delete;

	// Creates resources using the renderer services shared by all configured passes.
	[[nodiscard]]
	virtual bool Initialize(ID3D11Device* device, ShaderManager& shaders) = 0;

	// Copies per-submission values into callbacks owned by the graph until Reset.
	// This pass and borrowed renderer services must outlive those callbacks.
	[[nodiscard]]
	virtual bool AddToGraph(RenderGraph& graph, const RenderPassContext& context) const = 0;

protected:
	IRenderPass() = default;
};
