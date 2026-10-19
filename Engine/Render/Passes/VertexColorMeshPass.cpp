#include "VertexColorMeshPass.h"

#include <cstddef>
#include <utility>

#include "Engine/Core/Logging/Logger.h"
#include "Engine/Render/Mesh/MeshManager.h"
#include "Engine/Render/Mesh/Vertex.h"

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

std::optional<MeshPassCommands> VertexColorMeshPass::PrepareCommands(
	const RenderFrameData& frame,
	const RenderView& view,
	const MeshManager& meshes) const
{
	// A view must describe a usable viewport and invertible camera transform.
	if (view.extent.width == 0 || view.extent.height == 0)
	{
		GILGAMESH_LOG(Renderer, Error,
			"Cannot prepare mesh commands for an empty viewport");
		return std::nullopt;
	}

	const Matrix4 viewProjection = view.view * view.projection;
	Matrix4 inverseVP;
	if (!TryInverse(viewProjection, inverseVP))
	{
		GILGAMESH_LOG(Renderer, Error,
			"Cannot prepare mesh commands for a non-invertible view projection");
		return std::nullopt;
	}

	// Convert CPU row-vector matrices once while retaining constants per view.
	MeshPassCommands commands;
	commands.view.view = Transpose(view.view);
	commands.view.projection = Transpose(view.projection);
	commands.view.viewProjection = Transpose(viewProjection);
	commands.view.inverseVP = Transpose(inverseVP);
	commands.view.cameraWorldPos = view.cameraWorldPos;
	commands.view.viewportDim = {
		static_cast<float>(view.extent.width),
		static_cast<float>(view.extent.height)
	};

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

	// An empty scene still lets the graph perform attachment clear operations.
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
