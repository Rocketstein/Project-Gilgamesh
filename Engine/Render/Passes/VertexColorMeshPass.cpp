#include "VertexColorMeshPass.h"

#include <cstddef>
#include <stdexcept>
#include <utility>

#include "Engine/Core/Logging/Logger.h"
#include "Engine/Render/Mesh/MeshManager.h"
#include "Engine/Render/Mesh/Vertex.h"
#include "Engine/Render/RenderGraph/RenderGraph.h"

bool VertexColorMeshPass::Initialize(
	ID3D11Device* device,
	ShaderManager& shaders)
{
	// Resolve both shader resources before building a replacement draw setup.
	if (!device || !shaders.IsInitialized())
	{
		GILGAMESH_LOG(Renderer, Error,
			"Vertex-color mesh pass requires a device and initialized shaders");
		return false;
	}

	const auto vertexShader = shaders.LoadVertex(L"Primitive");
	const auto pixelShader = shaders.LoadPixel(L"Primitive");
	if (!vertexShader || !pixelShader)
	{
		GILGAMESH_LOG(Renderer, Error,
			"Failed to load vertex-color mesh pass shaders");
		return false;
	}

	const D3D11_INPUT_ELEMENT_DESC inputElements[] =
	{
		{
			"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,
			static_cast<UINT>(offsetof(NormalVertex, position)),
			D3D11_INPUT_PER_VERTEX_DATA, 0
		},
		{
			"COLOR", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,
			static_cast<UINT>(offsetof(NormalVertex, color)),
			D3D11_INPUT_PER_VERTEX_DATA, 0
		}
	};

	const DrawSetupDesc drawSetupDesc{
		.vertexShader = vertexShader.resource,
		.pixelShader = pixelShader.resource,
		.inputElements = inputElements,
		.drawState = {
			.topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST,
			.depthTestEnabled = true,
			.depthWriteEnabled = true,
			.depthComparison = D3D11_COMPARISON_LESS
		}
	};

	// Keep existing pass resources intact if any replacement allocation fails.
	DrawSetup newDrawSetup;
	ConstantBuffer<FrameConstants> newViewBuffer;
	ConstantBuffer<ObjectConstants> newObjectBuffer;
	if (FAILED(newDrawSetup.Initialize(device, shaders, drawSetupDesc)) ||
		!newViewBuffer.Create(device) || !newObjectBuffer.Create(device))
	{
		GILGAMESH_LOG(Renderer, Error,
			"Failed to create vertex-color mesh pass resources");
		return false;
	}

	drawSetup_ = std::move(newDrawSetup);
	viewCB_ = std::move(newViewBuffer);
	objectCB_ = std::move(newObjectBuffer);
	return true;
}

bool VertexColorMeshPass::AddToGraph(
	RenderGraph& graph,
	const RenderPassContext& context) const
{
	// Reject invalid submissions before adding any declarations to the graph.
	if (!IsInitialized() ||
		!graph.IsValid(context.color) || !graph.IsValid(context.depth))
	{
		GILGAMESH_LOG(Renderer, Error,
			"Cannot add a mesh pass without initialized resources and valid graph targets");
		return false;
	}
	auto commands = PrepareCommands(context.frame, context.view, context.meshes);
	if (!commands) return false;

	// Retain prepared values and stable services, never the temporary context.
	graph.AddPass(
		"VertexColorMeshes",
		[color = context.color, depth = context.depth](GraphPassBuilder& builder)
		{
			builder.SetColorTarget(0, color, RenderGraphLoadOp::Load);
			builder.SetDepthTarget(
				depth,
				RenderGraphDepthAccess::ReadWrite,
				RenderGraphLoadOp::Load);
		},
		[this, commands = std::move(*commands), meshes = &context.meshes](
			ID3D11DeviceContext& deviceContext)
		{
			// Propagate upload failures to the viewport renderer's execution boundary.
			if (!ExecuteCommands(&deviceContext, commands, *meshes))
			{
				throw std::runtime_error("Failed to execute vertex-color mesh commands");
			}
		});
	return true;
}

std::optional<MeshPassCommands> VertexColorMeshPass::PrepareCommands(
	const RenderFrameData& frame,
	const RenderView& view,
	const MeshManager& meshes) const
{
	// Meshes and editor overlays use the same validated view-constant layout.
	const auto viewConstants = PrepareViewConstants(view);
	if (!viewConstants) return std::nullopt;

	MeshPassCommands commands;
	commands.view = *viewConstants;

	// Commands own their transforms and retain only managed geometry handles.
	commands.draws.reserve(frame.staticMeshes.size());
	for (const ExtractedStaticMesh& instance : frame.staticMeshes)
	{
		if (!meshes.IsValid(instance.mesh)) continue;

		commands.draws.push_back(MeshDrawCommand{
			.mesh = instance.mesh,
			.object = { .model = Transpose(instance.localToWorld) }
		});
	}

	return commands;
}

bool VertexColorMeshPass::ExecuteCommands(
	ID3D11DeviceContext* context,
	const MeshPassCommands& commands,
	const MeshManager& meshes) const
{
	if (!context || !IsInitialized())
	{
		GILGAMESH_LOG(Renderer, Error,
			"Cannot execute an uninitialized vertex-color mesh pass or null context");
		return false;
	}

	// Empty scenes preserve the background produced by preceding passes.
	if (commands.draws.empty()) return true;

	// Upload at execution time so preparing another view cannot overwrite this one.
	if (!viewCB_.Update(context, commands.view))
	{
		GILGAMESH_LOG(Renderer, Error,
			"Failed to upload vertex-color mesh view constants");
		return false;
	}
	drawSetup_.Apply(context);
	ID3D11Buffer* viewBuffer = viewCB_.GetBuffer();
	ID3D11Buffer* objectBuffer = objectCB_.GetBuffer();
	context->VSSetConstantBuffers(0, 1, &viewBuffer);
	context->PSSetConstantBuffers(0, 1, &viewBuffer);
	context->VSSetConstantBuffers(2, 1, &objectBuffer);

	// Revalidate at submission because resources may be released after preparation.
	for (const MeshDrawCommand& command : commands.draws)
	{
		const StaticMesh* mesh = meshes.TryGet(command.mesh);
		if (!mesh) continue;

		if (!objectCB_.Update(context, command.object))
		{
			GILGAMESH_LOG(Renderer, Error,
				"Failed to upload vertex-color mesh object constants");
			return false;
		}
		mesh->Bind(context);
		context->DrawIndexed(mesh->GetIndexCount(), 0, 0);
	}
	return true;
}

bool VertexColorMeshPass::IsInitialized() const noexcept
{
	return drawSetup_.IsInitialized() &&
		viewCB_.IsInitialized() && objectCB_.IsInitialized();
}
