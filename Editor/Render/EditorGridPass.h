#pragma once

#include "Engine/Render/Buffers/ConstantBuffer.h"
#include "Engine/Render/DrawSetup/DrawSetup.h"
#include "Engine/Render/Passes/IRenderPass.h"
#include "Engine/Render/RenderGraph/Resource/RenderGraphTexture.h"
#include "Engine/Render/Renderer/RenderView.h"
#include "Engine/Render/Shader/ShaderConstants.h"

class RenderGraph;

// Draws an anti-aliased XY-plane background before opaque scene meshes.
class EditorGridPass final : public IRenderPass
{
public:
	EditorGridPass() = default;
	~EditorGridPass() override = default;

	EditorGridPass(const EditorGridPass&) = delete;
	EditorGridPass& operator=(const EditorGridPass&) = delete;
	EditorGridPass(EditorGridPass&&) = delete;
	EditorGridPass& operator=(EditorGridPass&&) = delete;

	// Publishes new GPU resources only after every creation succeeds.
	[[nodiscard]]
	bool Initialize(ID3D11Device* device, ShaderManager& shaders) override;

	// Copies valid appearance settings without changing already queued callbacks.
	[[nodiscard]]
	bool SetSettings(const EditorConstants& settings);

	// Returns the editor's initial grid appearance in world units and pixels.
	[[nodiscard]]
	static constexpr EditorConstants DefaultSettings() noexcept
	{
		return {
			.gridColor = { 0.45f, 0.48f, 0.52f, 0.65f },
			.gridFadeRange = { 10.0f, 100.0f },
			.gridSpacing = 1.0f,
			.gridWidth = 1.0f
		};
	}

	// Loads the cleared background and copies this view's settings into the graph.
	// The grid does not consume scene depth; this pass must outlive its callbacks.
	[[nodiscard]]
	bool AddToGraph(
		RenderGraph& graph,
		const RenderPassContext& context) const override;

	[[nodiscard]]
	bool IsInitialized() const noexcept;

private:
	// Uploads copied constants at execution and submits a procedural triangle.
	[[nodiscard]]
	bool ExecuteCommands(
		ID3D11DeviceContext* context,
		const FrameConstants& view,
		const EditorConstants& settings) const;

private:
	DrawSetup drawSetup_; // Full-screen shader and alpha blend with depth testing disabled.
	ConstantBuffer<FrameConstants> viewCB_; // Updated for the view being executed.
	ConstantBuffer<EditorConstants> editorCB_; // Grid appearance copied for each pass.
	EditorConstants settings_ = DefaultSettings(); // Validated appearance for future submissions.
};
