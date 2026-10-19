#pragma once

#include <optional>
#include <vector>

#include "Engine/Render/Buffers/ConstantBuffer.h"
#include "Engine/Render/DrawSetup/DrawSetup.h"
#include "Engine/Render/Renderer/RenderFrameData.h"
#include "Engine/Render/Renderer/RenderView.h"
#include "Engine/Render/Shader/ShaderConstants.h"
#include "IRenderPass.h"

class MeshManager;

// Owns the copied instance data needed for one whole-mesh indexed draw.
struct MeshDrawCommand
{
	MeshHandle mesh; // Resolved again at execution to reject stale resources.
	ObjectConstants object{}; // Model matrix in shader-upload layout.
};

// Owns the CPU data for one view of the vertex-color mesh pass.
struct MeshPassCommands
{
	FrameConstants view{}; // View constants in shader-upload layout.
	std::vector<MeshDrawCommand> draws; // Preserves extraction order.
};

// Prepares copied mesh commands and submits them through fixed vertex-color state.
class VertexColorMeshPass final : public IRenderPass
{
public:
	VertexColorMeshPass() = default;
	~VertexColorMeshPass() override = default;

	VertexColorMeshPass(const VertexColorMeshPass&) = delete;
	VertexColorMeshPass& operator=(const VertexColorMeshPass&) = delete;
	VertexColorMeshPass(VertexColorMeshPass&&) = delete;
	VertexColorMeshPass& operator=(VertexColorMeshPass&&) = delete;

	// Publishes new GPU state only after every required resource is created.
	[[nodiscard]]
	bool Initialize(ID3D11Device* device, ShaderManager& shaders) override;

	// Prepares copied commands and loads targets initialized by an earlier pass.
	[[nodiscard]]
	bool AddToGraph(RenderGraph& graph, const RenderPassContext& context) const override;

	// Copies one extracted snapshot for a view without changing GPU state.
	// This CPU-only operation does not require Initialize to have been called.
	[[nodiscard]]
	std::optional<MeshPassCommands> PrepareCommands(
		const RenderFrameData& frame,
		const RenderView& view,
		const MeshManager& meshes) const;

	// Updates and binds pass-owned buffers, resolves meshes, and submits draws.
	// The render graph supplies attachments, clears, and the viewport beforehand.
	[[nodiscard]]
	bool ExecuteCommands(
		ID3D11DeviceContext* context,
		const MeshPassCommands& commands,
		const MeshManager& meshes) const;

	[[nodiscard]]
	bool IsInitialized() const noexcept;

private:
	DrawSetup drawSetup_; // Fixed shaders, vertex layout, and draw state.
	ConstantBuffer<FrameConstants> viewCB_; // Updated only when a view executes.
	ConstantBuffer<ObjectConstants> objectCB_; // Reused for each submitted instance.
};
