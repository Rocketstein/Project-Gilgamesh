#pragma once

#include "Engine/Render/Buffers/ConstantBuffer.h"
#include "Engine/Render/DrawSetup/DrawSetup.h"
#include "Engine/Render/RenderGraph/Resource/RenderGraphTexture.h"
#include "Engine/Render/Renderer/RenderView.h"
#include "Engine/Render/Shader/ShaderConstants.h"

class RenderGraph;

// Draws an anti-aliased XY-plane grid using the mesh pass's existing depth.
class EditorGridPass final
{
public:
	EditorGridPass() = default;
	~EditorGridPass() = default;

	EditorGridPass(const EditorGridPass&) = delete;
	EditorGridPass& operator=(const EditorGridPass&) = delete;
	EditorGridPass(EditorGridPass&&) = delete;
	EditorGridPass& operator=(EditorGridPass&&) = delete;

	// Publishes new GPU resources only after every creation succeeds.
	[[nodiscard]]
	bool Initialize(ID3D11Device* device, ShaderManager& shaders);

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

	// Copies view and grid settings into the graph; this pass must outlive it.
	// Color and depth must already contain the scene pass's clear or draw results.
	[[nodiscard]]
	bool AddToGraph(
		RenderGraph& graph,
		RenderGraphTextureHandle color,
		RenderGraphTextureHandle depth,
		const RenderView& view,
		const EditorConstants& settings = DefaultSettings()) const;

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
	DrawSetup drawSetup_; // Full-screen shader, alpha blend, and read-only depth state.
	ConstantBuffer<FrameConstants> viewCB_; // Updated for the view being executed.
	ConstantBuffer<EditorConstants> editorCB_; // Grid appearance copied for each pass.
};
