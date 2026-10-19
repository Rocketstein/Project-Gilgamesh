#include "EditorGridPass.h"

#include <cmath>
#include <stdexcept>
#include <utility>

#include "Engine/Core/Logging/Logger.h"
#include "Engine/Render/RenderGraph/RenderGraph.h"

namespace
{
	// Reject undefined shader arithmetic before recording a graph callback.
	bool IsGridSettingsValid(const EditorConstants& settings) noexcept
	{
		return std::isfinite(settings.gridColor.x) &&
			std::isfinite(settings.gridColor.y) &&
			std::isfinite(settings.gridColor.z) &&
			std::isfinite(settings.gridColor.w) &&
			settings.gridColor.w >= 0.0f && settings.gridColor.w <= 1.0f &&
			std::isfinite(settings.gridFadeRange.x) &&
			std::isfinite(settings.gridFadeRange.y) &&
			settings.gridFadeRange.x >= 0.0f &&
			settings.gridFadeRange.y > settings.gridFadeRange.x &&
			std::isfinite(settings.gridSpacing) && settings.gridSpacing > 0.0f &&
			std::isfinite(settings.gridWidth) && settings.gridWidth > 0.0f;
	}
} // Anonymous namespace

bool EditorGridPass::Initialize(ID3D11Device* device, ShaderManager& shaders)
{
	// Resolve the procedural shaders before building replacement pass resources.
	if (!device || !shaders.IsInitialized())
	{
		GILGAMESH_LOG(Renderer, Error,
			"Editor grid pass requires a device and initialized shaders");
		return false;
	}
	const auto vertexShader = shaders.LoadVertex(L"EditorGrid");
	const auto pixelShader = shaders.LoadPixel(L"EditorGrid");
	if (!vertexShader || !pixelShader)
	{
		GILGAMESH_LOG(Renderer, Error, "Failed to load editor grid shaders");
		return false;
	}

	// The vertex shader generates a full-screen triangle without a vertex layout.
	const DrawSetupDesc description{
		.vertexShader = vertexShader.resource,
		.pixelShader = pixelShader.resource,
		.inputElements = {},
		.drawState = {
			.topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST,
			.cullMode = D3D11_CULL_NONE,
			.depthTestEnabled = false,
			.depthWriteEnabled = false,
			.blendEnabled = true
		}
	};

	// Preserve the previous initialized pass if any replacement allocation fails.
	DrawSetup newDrawSetup;
	ConstantBuffer<FrameConstants> newViewBuffer;
	ConstantBuffer<EditorConstants> newEditorBuffer;
	if (FAILED(newDrawSetup.Initialize(device, shaders, description)) ||
		!newViewBuffer.Create(device) || !newEditorBuffer.Create(device))
	{
		GILGAMESH_LOG(Renderer, Error, "Failed to create editor grid resources");
		return false;
	}

	drawSetup_ = std::move(newDrawSetup);
	viewCB_ = std::move(newViewBuffer);
	editorCB_ = std::move(newEditorBuffer);
	return true;
}

// Keep invalid shader arithmetic out of future submissions while preserving current settings.
bool EditorGridPass::SetSettings(const EditorConstants& settings)
{
	if (!IsGridSettingsValid(settings))
	{
		GILGAMESH_LOG(Renderer, Error, "Cannot set an editor grid with invalid settings");
		return false;
	}
	settings_ = settings;
	return true;
}

bool EditorGridPass::AddToGraph(RenderGraph& graph, const RenderPassContext& context) const
{
	// Fail before adding declarations when this submission cannot be executed.
	if (!IsInitialized() || !graph.IsValid(context.color))
	{
		GILGAMESH_LOG(Renderer, Error,
			"Cannot add an editor grid without initialized resources and a valid color target");
		return false;
	}
	const auto viewConstants = PrepareViewConstants(context.view);
	if (!viewConstants) return false;

	// Blend over the cleared background; later opaque meshes cover this color.
	graph.AddPass(
		"EditorGrid",
		[color = context.color](GraphPassBuilder& builder)
		{
			builder.SetColorTarget(0, color, RenderGraphLoadOp::Load);
		},
		[this, constants = *viewConstants, settings = settings_](ID3D11DeviceContext& deviceContext)
		{
			// Each callback owns its CPU data until this graph is reset.
			if (!ExecuteCommands(&deviceContext, constants, settings))
			{
				throw std::runtime_error("Failed to execute editor grid commands");
			}
		});
	return true;
}

bool EditorGridPass::ExecuteCommands(
	ID3D11DeviceContext* context,
	const FrameConstants& view,
	const EditorConstants& settings) const
{
	if (!context || !IsInitialized())
	{
		GILGAMESH_LOG(Renderer, Error,
			"Cannot execute an uninitialized editor grid pass or null context");
		return false;
	}

	// Upload only during execution so preparing another view cannot overwrite it.
	if (!viewCB_.Update(context, view) || !editorCB_.Update(context, settings))
	{
		GILGAMESH_LOG(Renderer, Error, "Failed to upload editor grid constants");
		return false;
	}
	drawSetup_.Apply(context);
	ID3D11Buffer* viewBuffer = viewCB_.GetBuffer();
	ID3D11Buffer* editorBuffer = editorCB_.GetBuffer();
	context->PSSetConstantBuffers(0, 1, &viewBuffer);
	context->PSSetConstantBuffers(1, 1, &editorBuffer);

	// The pixel shader reconstructs Z=0 intersections only to shade the background.
	context->Draw(3, 0);
	return true;
}

bool EditorGridPass::IsInitialized() const noexcept
{
	return drawSetup_.IsInitialized() &&
		viewCB_.IsInitialized() && editorCB_.IsInitialized();
}
