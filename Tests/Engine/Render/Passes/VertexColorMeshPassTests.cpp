#include "Engine/ECS/Component/MeshComponent.h"
#include "Engine/ECS/Component/TransformComponent.h"
#include "Engine/ECS/Framework/World.h"
#include "Engine/ECS/System/RenderExtractionSystem.h"
#include "Engine/ECS/System/Transform/LocalToWorldSystem.h"
#include "Engine/ECS/System/Transform/LocalTransformSystem.h"
#include "Engine/ECS/System/Transform/TransformHierarchySystem.h"
#include "Engine/Render/Mesh/MeshManager.h"
#include "Engine/Render/Passes/VertexColorMeshPass.h"
#include "Engine/Render/Passes/ViewportClearPass.h"
#include "Engine/Render/RenderGraph/RenderGraph.h"
#include "Tests/TestFramework.h"

#include <array>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <utility>

#include <Windows.h>
#include <d3d11.h>
#include <wrl/client.h>

namespace
{
	// Shares a software device while each test clears the immediate context state.
	struct TestDevice
	{
		Microsoft::WRL::ComPtr<ID3D11Device> device;
		Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
	};

	// WARP makes shader, buffer, and pixel checks independent of physical hardware.
	const TestDevice& GetTestDevice()
	{
		static const TestDevice device = []
		{
			TestDevice result;
			const HRESULT status = D3D11CreateDevice(
				nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, nullptr, 0,
				D3D11_SDK_VERSION, result.device.GetAddressOf(), nullptr,
				result.context.GetAddressOf());
			if (FAILED(status))
			{
				result.device.Reset();
				result.context.Reset();
			}
			return result;
		}();
		return device;
	}

	// Shader assets are registered separately for the test executable.
	std::filesystem::path GetShaderDirectory()
	{
		std::array<wchar_t, MAX_PATH> executablePath{};
		const DWORD length = GetModuleFileNameW(
			nullptr, executablePath.data(),
			static_cast<DWORD>(executablePath.size()));
		if (length == 0 || length == executablePath.size()) return {};
		return std::filesystem::path{
			std::wstring_view{ executablePath.data(), length }
		}.parent_path() / L"Shaders";
	}

	// Initializes the resource owners used by a real pass execution.
	bool InitializePass(
		Gilgamesh::Testing::TestContext& testContext,
		VertexColorMeshPass& pass,
		ShaderManager& shaders,
		MeshManager& meshes)
	{
		const TestDevice& device = GetTestDevice();
		const bool initialized = device.device && device.context &&
			shaders.Initialize(device.device.Get(), GetShaderDirectory()) &&
			meshes.Initialize(device.device.Get()) &&
			pass.Initialize(device.device.Get(), shaders);
		GILGAMESH_CHECK_MESSAGE(initialized,
			"the WARP device, shaders, mesh manager, and mesh pass must initialize");
		if (device.context) device.context->ClearState();
		return initialized;
	}

	// A uniform vertex color gives an unambiguous pixel after the indexed draw.
	MeshHandle CreateGreenTriangle(MeshManager& meshes)
	{
		StaticMeshData data;
		data.vertices_.resize(3);
		data.vertices_[0].position = { -0.75f, -0.75f, 0.5f };
		data.vertices_[1].position = { 0.0f, 0.75f, 0.5f };
		data.vertices_[2].position = { 0.75f, -0.75f, 0.5f };
		for (NormalVertex& vertex : data.vertices_)
		{
			vertex.color = { 0.0f, 1.0f, 0.0f, 1.0f };
		}
		data.indices_ = { 0, 1, 2 };
		return meshes.CreateMesh(data);
	}

	// Read back the actual uploaded buffer rather than its CPU-side input value.
	template<typename Constants>
	bool ReadVertexConstants(UINT slot, Constants& constants)
	{
		const TestDevice& device = GetTestDevice();
		Microsoft::WRL::ComPtr<ID3D11Buffer> source;
		device.context->VSGetConstantBuffers(slot, 1, source.GetAddressOf());
		if (!source) return false;

		D3D11_BUFFER_DESC description{};
		source->GetDesc(&description);
		if (description.ByteWidth != sizeof(Constants)) return false;
		description.Usage = D3D11_USAGE_STAGING;
		description.BindFlags = 0;
		description.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
		description.MiscFlags = 0;
		description.StructureByteStride = 0;
		Microsoft::WRL::ComPtr<ID3D11Buffer> staging;
		if (FAILED(device.device->CreateBuffer(
			&description, nullptr, staging.GetAddressOf()))) return false;

		device.context->CopyResource(staging.Get(), source.Get());
		D3D11_MAPPED_SUBRESOURCE mapped{};
		if (FAILED(device.context->Map(
			staging.Get(), 0, D3D11_MAP_READ, 0, &mapped))) return false;
		std::memcpy(&constants, mapped.pData, sizeof(Constants));
		device.context->Unmap(staging.Get(), 0);
		return true;
	}

	// Copy the center RGBA8 pixel from the graph output into CPU-readable storage.
	std::optional<std::array<std::uint8_t, 4>> ReadCenterPixel(
		ID3D11Texture2D* texture)
	{
		const TestDevice& device = GetTestDevice();
		D3D11_TEXTURE2D_DESC description{};
		texture->GetDesc(&description);
		description.Usage = D3D11_USAGE_STAGING;
		description.BindFlags = 0;
		description.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
		description.MiscFlags = 0;
		Microsoft::WRL::ComPtr<ID3D11Texture2D> staging;
		if (FAILED(device.device->CreateTexture2D(
			&description, nullptr, staging.GetAddressOf()))) return std::nullopt;

		device.context->CopyResource(staging.Get(), texture);
		D3D11_MAPPED_SUBRESOURCE mapped{};
		if (FAILED(device.context->Map(
			staging.Get(), 0, D3D11_MAP_READ, 0, &mapped))) return std::nullopt;
		const auto* source = static_cast<const std::uint8_t*>(mapped.pData) +
			(description.Height / 2) * mapped.RowPitch +
			(description.Width / 2) * 4;
		std::array<std::uint8_t, 4> result{};
		std::memcpy(result.data(), source, result.size());
		device.context->Unmap(staging.Get(), 0);
		return result;
	}

	// The graph owns its moved command list until execution, including empty lists.
	std::optional<std::array<std::uint8_t, 4>> RenderCenterPixel(
		const VertexColorMeshPass& pass,
		const MeshManager& meshes,
		MeshPassCommands commands)
	{
		const TestDevice& device = GetTestDevice();
		device.context->ClearState();
		RenderGraph graph;
		const auto color = graph.CreateTexture("PassColor", {
			.extent = { 64, 64 },
			.format = DXGI_FORMAT_R8G8B8A8_UNORM
		});
		const auto depth = graph.CreateTexture("PassDepth", {
			.extent = { 64, 64 },
			.format = DXGI_FORMAT_D32_FLOAT
		});
		ViewportClearPass clear{ Color4{ 0.0f, 0.0f, 0.0f, 1.0f } };
		ShaderManager shaders;
		RenderFrameData frame;
		const RenderView view{ .extent = { 64, 64 } };
		if (!clear.Initialize(device.device.Get(), shaders)) return std::nullopt;
		if (!clear.AddToGraph(graph, { color, depth, frame, view, meshes }))
			return std::nullopt;
		Microsoft::WRL::ComPtr<ID3D11Texture2D> output;
		graph.AddPass(
			"VertexColorMeshes",
			[=](GraphPassBuilder& builder)
			{
				builder.SetColorTarget(0, color, RenderGraphLoadOp::Load);
				builder.SetDepthTarget(depth, RenderGraphDepthAccess::ReadWrite,
					RenderGraphLoadOp::Load);
			},
			[&pass, &meshes, &output, commands = std::move(commands)]
			(ID3D11DeviceContext& context)
			{
				if (!pass.ExecuteCommands(&context, commands, meshes))
				{
					throw std::runtime_error("the mesh pass execution failed");
				}

				// Retain the graph output solely for readback after attachment cleanup.
				Microsoft::WRL::ComPtr<ID3D11RenderTargetView> target;
				context.OMGetRenderTargets(1, target.GetAddressOf(), nullptr);
				Microsoft::WRL::ComPtr<ID3D11Resource> resource;
				if (target) target->GetResource(resource.GetAddressOf());
				if (resource) (void)resource.As(&output);
			});
		if (!graph.Compile(device.device.Get()) ||
			!graph.Execute(device.context.Get()) || !output) return std::nullopt;
		device.context->ClearState();
		return ReadCenterPixel(output.Get());
	}
} // Anonymous namespace

GILGAMESH_TEST("Engine.Render.VertexColorMeshPass", "RejectsInvalidInitialization")
{
	const TestDevice& device = GetTestDevice();
	ShaderManager shaders;
	VertexColorMeshPass pass;
	GILGAMESH_CHECK_MESSAGE(!pass.Initialize(nullptr, shaders),
		"initialization must reject a null device");
	if (device.device)
	{
		GILGAMESH_CHECK_MESSAGE(!pass.Initialize(device.device.Get(), shaders),
			"initialization must reject an uninitialized shader manager");
	}
	GILGAMESH_CHECK_MESSAGE(!pass.IsInitialized(),
		"failed initialization must not publish a partial pass");
}

GILGAMESH_TEST("Engine.Render.VertexColorMeshPass", "PreparesOwnedCommandsAndSeparateViewConstants")
{
	MeshManager meshes;
	const TestDevice& device = GetTestDevice();
	if (!device.device || !meshes.Initialize(device.device.Get())) return;
	const MeshHandle mesh = CreateGreenTriangle(meshes);
	const MeshHandle stale = CreateGreenTriangle(meshes);
	GILGAMESH_CHECK_MESSAGE(mesh && stale && meshes.RemoveMesh(stale),
		"the fixture must provide live and stale mesh handles");
	if (!mesh || !stale) return;

	Matrix4 model = Matrix4::Identity();
	model(3, 0) = 2.0f;
	model(3, 1) = 3.0f;
	Matrix4 view = Matrix4::Identity();
	view(0, 0) = 2.0f;
	view(1, 1) = 3.0f;
	Matrix4 projection = Matrix4::Identity();
	projection(3, 0) = -4.0f;
	const RenderView renderView{
		.view = view,
		.projection = projection,
		.cameraWorldPos = { 5.0f, 6.0f, 7.0f },
		.extent = { 320, 180 }
	};
	RenderFrameData frame;
	frame.staticMeshes = { { mesh, model }, { stale, {} }, { {}, {} } };

	// Preparation only copies CPU data; an initialized GPU pass is unnecessary.
	VertexColorMeshPass pass;
	auto commands = pass.PrepareCommands(frame, renderView, meshes);
	GILGAMESH_CHECK_MESSAGE(commands && commands->draws.size() == 1,
		"preparation must retain only live meshes in their original order");
	if (!commands || commands->draws.size() != 1) return;
	frame.staticMeshes.clear();
	GILGAMESH_CHECK_MESSAGE(commands->draws[0].mesh == mesh &&
		NearlyEquals(commands->draws[0].object.model, Transpose(model)),
		"commands must own the model matrix without folding in camera transforms");
	Matrix4 inverseVP;
	GILGAMESH_CHECK_MESSAGE(TryInverse(view * projection, inverseVP) &&
		NearlyEquals(commands->view.view, Transpose(view)) &&
		NearlyEquals(commands->view.projection, Transpose(projection)) &&
		NearlyEquals(commands->view.viewProjection, Transpose(view * projection)) &&
		NearlyEquals(commands->view.inverseVP, Transpose(inverseVP)) &&
		commands->view.cameraWorldPos == renderView.cameraWorldPos &&
		commands->view.viewportDim == (Vector2{ 320.0f, 180.0f }),
		"view constants must contain the complete independent shader upload layout");
}

GILGAMESH_TEST("Engine.Render.VertexColorMeshPass", "RejectsViewsWithoutAUsableProjectionOrExtent")
{
	VertexColorMeshPass pass;
	MeshManager meshes;
	RenderFrameData frame;
	RenderView view;
	GILGAMESH_CHECK_MESSAGE(!pass.PrepareCommands(frame, view, meshes),
		"a zero-sized view must not publish commands");
	view.extent = { 64, 64 };
	view.projection(0, 0) = 0.0f;
	GILGAMESH_CHECK_MESSAGE(!pass.PrepareCommands(frame, view, meshes),
		"a singular view-projection must not publish invalid inverse constants");
}

GILGAMESH_TEST("Engine.Render.VertexColorMeshPass", "ExecutesConfiguredStateAndUploadsB0AndB2")
{
	VertexColorMeshPass pass;
	ShaderManager shaders;
	MeshManager meshes;
	if (!InitializePass(testContext, pass, shaders, meshes)) return;
	const TestDevice& device = GetTestDevice();
	const MeshHandle mesh = CreateGreenTriangle(meshes);
	RenderFrameData frame;
	Matrix4 model = Matrix4::Identity();
	model(3, 0) = 2.0f;
	frame.staticMeshes.push_back({ mesh, model });
	RenderView view{ .extent = { 64, 64 } };
	view.view(3, 1) = 3.0f;
	view.projection(0, 0) = 4.0f;
	auto commands = pass.PrepareCommands(frame, view, meshes);
	GILGAMESH_CHECK_MESSAGE(commands.has_value(),
		"valid CPU data must produce a command list");
	if (!commands) return;
	GILGAMESH_CHECK_MESSAGE(pass.ExecuteCommands(device.context.Get(), *commands, meshes),
		"the prepared command list must execute");

	// Inspect actual bound state after deliberately starting from an empty context.
	D3D11_PRIMITIVE_TOPOLOGY topology{};
	device.context->IAGetPrimitiveTopology(&topology);
	Microsoft::WRL::ComPtr<ID3D11InputLayout> inputLayout;
	device.context->IAGetInputLayout(inputLayout.GetAddressOf());
	Microsoft::WRL::ComPtr<ID3D11VertexShader> vertexShader;
	Microsoft::WRL::ComPtr<ID3D11PixelShader> pixelShader;
	device.context->VSGetShader(vertexShader.GetAddressOf(), nullptr, nullptr);
	device.context->PSGetShader(pixelShader.GetAddressOf(), nullptr, nullptr);
	Microsoft::WRL::ComPtr<ID3D11DepthStencilState> depthState;
	device.context->OMGetDepthStencilState(depthState.GetAddressOf(), nullptr);
	Microsoft::WRL::ComPtr<ID3D11RasterizerState> rasterizer;
	device.context->RSGetState(rasterizer.GetAddressOf());
	Microsoft::WRL::ComPtr<ID3D11BlendState> blend;
	device.context->OMGetBlendState(blend.GetAddressOf(), nullptr, nullptr);
	GILGAMESH_CHECK_MESSAGE(topology == D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST &&
		inputLayout && vertexShader && pixelShader && depthState && rasterizer && blend,
		"execution must apply every configured shader and fixed-function state");
	if (depthState && rasterizer && blend)
	{
		D3D11_DEPTH_STENCIL_DESC depthDescription{};
		D3D11_RASTERIZER_DESC rasterDescription{};
		D3D11_BLEND_DESC blendDescription{};
		depthState->GetDesc(&depthDescription);
		rasterizer->GetDesc(&rasterDescription);
		blend->GetDesc(&blendDescription);
		GILGAMESH_CHECK_MESSAGE(depthDescription.DepthEnable &&
			depthDescription.DepthWriteMask == D3D11_DEPTH_WRITE_MASK_ALL &&
			depthDescription.DepthFunc == D3D11_COMPARISON_LESS &&
			rasterDescription.FillMode == D3D11_FILL_SOLID &&
			rasterDescription.CullMode == D3D11_CULL_BACK &&
			!blendDescription.RenderTarget[0].BlendEnable,
			"the mesh pass must use solid opaque rendering with writable LESS depth");
	}

	// Independent slots must retain model and camera transforms separately.
	FrameConstants uploadedView{};
	ObjectConstants uploadedObject{};
	GILGAMESH_CHECK_MESSAGE(ReadVertexConstants(0, uploadedView) &&
		ReadVertexConstants(2, uploadedObject) &&
		NearlyEquals(uploadedView.viewProjection, Transpose(view.view * view.projection)) &&
		NearlyEquals(uploadedObject.model, Transpose(model)),
		"execution must upload camera constants to b0 and only the model matrix to b2");
	device.context->ClearState();
}

GILGAMESH_TEST("Engine.Render.VertexColorMeshPass", "RendersExtractedWorldAfterSourceWorldIsDestroyed")
{
	VertexColorMeshPass pass;
	ShaderManager shaders;
	MeshManager meshes;
	if (!InitializePass(testContext, pass, shaders, meshes)) return;
	const MeshHandle mesh = CreateGreenTriangle(meshes);
	std::optional<RenderFrameData> frame;
	{
		World world;
		// Extraction consumes evaluated world matrices supplied by the registered transform pipeline.
		auto hierarchy = std::make_unique<TransformHierarchySystem>();
		TransformHierarchySystem& hierarchyReference = *hierarchy;
		world.GetTransformSystems().AddSystem(std::move(hierarchy)).SetEnabled(true);
		world.GetTransformSystems().AddSystem(std::make_unique<LocalTransformSystem>()).SetEnabled(true);
		world.GetTransformSystems().AddSystem(
			std::make_unique<LocalToWorldSystem>(hierarchyReference)).SetEnabled(true);
		auto extractor = std::make_unique<RenderExtractionSystem>();
		RenderExtractionSystem& observer = *extractor;
		extractor->SetEnabled(true);
		world.GetExtractionSystems().AddSystem(std::move(extractor));
		const SceneHandle scene = world.CreateScene("RenderedScene");
		const Entity entity = world.CreateEntity(scene);
		TransformComponent transform;
		transform.position_.x = -1.0f;
		world.Emplace<TransformComponent>(entity, transform);
		world.Emplace<MeshComponent>(entity, MeshComponent{ mesh });
		GILGAMESH_CHECK_MESSAGE(world.RequestSetActiveScene(scene),
			"the rendered scene must be selected before extraction");
		world.PreparePresentation(0.0f);
		frame = observer.TakeFrame();
	}
	GILGAMESH_CHECK_MESSAGE(frame && frame->staticMeshes.size() == 1,
		"the consumer must retain the extracted instance beyond world lifetime");
	if (!frame || frame->staticMeshes.size() != 1) return;

	// Camera translation cancels the extracted model translation in the shader.
	RenderView view{ .extent = { 64, 64 } };
	view.view(3, 0) = 1.0f;
	auto commands = pass.PrepareCommands(*frame, view, meshes);
	frame.reset();
	GILGAMESH_CHECK_MESSAGE(commands.has_value(),
		"an independent extraction snapshot must produce render commands");
	if (!commands) return;
	const auto pixel = RenderCenterPixel(pass, meshes, std::move(*commands));
	const std::array<std::uint8_t, 4> green{ 0, 255, 0, 255 };
	GILGAMESH_CHECK_MESSAGE(pixel && *pixel == green,
		"world extraction must reach an actual indexed draw through graph execution");
}

GILGAMESH_TEST("Engine.Render.VertexColorMeshPass", "SkipsStaleMeshesAndPreservesExplicitClear")
{
	VertexColorMeshPass pass;
	ShaderManager shaders;
	MeshManager meshes;
	if (!InitializePass(testContext, pass, shaders, meshes)) return;
	const MeshHandle mesh = CreateGreenTriangle(meshes);
	RenderFrameData frame;
	frame.staticMeshes.push_back({ mesh, {} });
	const RenderView view{ .extent = { 64, 64 } };
	auto commands = pass.PrepareCommands(frame, view, meshes);
	if (!commands) return;
	GILGAMESH_CHECK_MESSAGE(meshes.RemoveMesh(mesh),
		"the prepared mesh must be removable before GPU execution");
	const MeshHandle replacement = CreateGreenTriangle(meshes);
	GILGAMESH_CHECK_MESSAGE(replacement && replacement != mesh,
		"reusing a mesh slot must issue a new generation");
	const std::array<std::uint8_t, 4> black{ 0, 0, 0, 255 };
	const auto stalePixel = RenderCenterPixel(pass, meshes, std::move(*commands));
	GILGAMESH_CHECK_MESSAGE(stalePixel && *stalePixel == black,
		"execution must not draw a replacement resource through a stale command handle");

	// An empty extraction preserves the clear submitted by a separate viewport pass.
	frame.staticMeshes.clear();
	auto empty = pass.PrepareCommands(frame, view, meshes);
	GILGAMESH_CHECK_MESSAGE(empty && empty->draws.empty(),
		"an empty scene must produce a valid empty command list");
	if (!empty) return;
	const auto emptyPixel = RenderCenterPixel(pass, meshes, std::move(*empty));
	GILGAMESH_CHECK_MESSAGE(emptyPixel && *emptyPixel == black,
		"an empty mesh pass must preserve the independent viewport clear");
}

GILGAMESH_TEST("Engine.Render.VertexColorMeshPass", "UploadsEachViewWhenItsCommandsExecute")
{
	VertexColorMeshPass pass;
	ShaderManager shaders;
	MeshManager meshes;
	if (!InitializePass(testContext, pass, shaders, meshes)) return;
	const TestDevice& device = GetTestDevice();
	RenderFrameData frame;
	frame.staticMeshes.push_back({ CreateGreenTriangle(meshes), {} });
	RenderView firstView{ .extent = { 64, 64 } };
	firstView.view(3, 0) = 1.0f;
	RenderView secondView{ .extent = { 128, 96 } };
	secondView.view(3, 1) = 2.0f;
	auto first = pass.PrepareCommands(frame, firstView, meshes);
	auto second = pass.PrepareCommands(frame, secondView, meshes);
	GILGAMESH_CHECK_MESSAGE(first && second,
		"a shared snapshot must independently prepare commands for both views");
	if (!first || !second) return;
	Microsoft::WRL::ComPtr<ID3D11Buffer> beforeExecution;
	device.context->VSGetConstantBuffers(0, 1, beforeExecution.GetAddressOf());
	GILGAMESH_CHECK_MESSAGE(!beforeExecution,
		"preparing multiple views must not alter GPU constant bindings");

	// Reverse execution order proves each list carries its own view upload data.
	FrameConstants uploaded{};
	GILGAMESH_CHECK_MESSAGE(pass.ExecuteCommands(device.context.Get(), *second, meshes) &&
		ReadVertexConstants(0, uploaded) &&
		NearlyEquals(uploaded.view, Transpose(secondView.view)) &&
		uploaded.viewportDim == (Vector2{ 128.0f, 96.0f }),
		"the second view must upload only when its commands execute");
	GILGAMESH_CHECK_MESSAGE(pass.ExecuteCommands(device.context.Get(), *first, meshes) &&
		ReadVertexConstants(0, uploaded) &&
		NearlyEquals(uploaded.view, Transpose(firstView.view)) &&
		uploaded.viewportDim == (Vector2{ 64.0f, 64.0f }),
		"executing the first list later must restore that list's own camera constants");
	device.context->ClearState();
}
