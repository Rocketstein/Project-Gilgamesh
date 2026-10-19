#include "Editor/EditorViewport/EditorViewport.h"
#include "Editor/Render/EditorGridPass.h"
#include "Engine/ECS/Framework/World.h"
#include "Engine/ECS/System/RenderExtractionSystem.h"
#include "Engine/Render/Mesh/MeshManager.h"
#include "Engine/Render/Passes/VertexColorMeshPass.h"
#include "Engine/Render/RenderGraph/RenderGraph.h"
#include "Tests/TestFramework.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <limits>
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
	// Reuses a software device; each test starts from cleared immediate state.
	struct TestDevice
	{
		Microsoft::WRL::ComPtr<ID3D11Device> device;
		Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
	};

	// Pixel checks run on WARP without requiring physical graphics hardware.
	const TestDevice& GetTestDevice()
	{
		static const TestDevice device = []
		{
			TestDevice result;
			(void)D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr,
				0, nullptr, 0, D3D11_SDK_VERSION, result.device.GetAddressOf(),
				nullptr, result.context.GetAddressOf());
			return result;
		}();
		return device;
	}

	// Tests load their own separately registered compiled shader assets.
	std::filesystem::path GetShaderDirectory()
	{
		std::array<wchar_t, MAX_PATH> executablePath{};
		const DWORD length = GetModuleFileNameW(nullptr, executablePath.data(),
			static_cast<DWORD>(executablePath.size()));
		if (length == 0 || length == executablePath.size()) return {};
		return std::filesystem::path{
			std::wstring_view{ executablePath.data(), length }
		}.parent_path() / L"Shaders";
	}

	// Initializes the real overlay resources and viewport attachments.
	bool InitializePass(Gilgamesh::Testing::TestContext& testContext,
		EditorGridPass& pass, ShaderManager& shaders, EditorViewport& viewport,
		Extent2D extent = { 64, 64 })
	{
		const TestDevice& device = GetTestDevice();
		const bool initialized = device.device && device.context &&
			shaders.Initialize(device.device.Get(), GetShaderDirectory()) &&
			pass.Initialize(device.device.Get(), shaders);
		GILGAMESH_CHECK_MESSAGE(initialized,
			"the WARP device, shaders, and grid pass must initialize");
		if (!initialized) return false;
		device.context->ClearState();
		viewport.Initialize(device.device.Get());
		viewport.RequestResize(extent);
		const bool ready = viewport.ApplyPendingResize() && viewport.IsReady();
		GILGAMESH_CHECK_MESSAGE(ready,
			"the viewport must provide all color and depth attachment views");
		return ready;
	}

	// Places the center pixel precisely on both XY grid lines at depth 0.5.
	RenderView MakeGridView(Extent2D extent)
	{
		RenderView view{ .cameraWorldPos = { 0.0f, 0.0f, -1.0f }, .extent = extent };
		view.view(3, 0) = 1.0f / static_cast<float>(extent.width);
		view.view(3, 1) = -1.0f / static_cast<float>(extent.height);
		view.view(3, 2) = 0.5f;
		return view;
	}

	// Disables distance fading so a centered line has exactly half opacity.
	EditorConstants MakeGridSettings()
	{
		return {
			.gridColor = { 1.0f, 0.0f, 0.0f, 0.5f },
			.gridFadeRange = { 100.0f, 200.0f },
			.gridSpacing = 0.25f,
			.gridWidth = 1.0f
		};
	}

	// Imports the same viewport depth texture with both attachment access modes.
	std::pair<RenderGraphTextureHandle, RenderGraphTextureHandle> ImportViewport(
		RenderGraph& graph, const EditorViewport& viewport)
	{
		const auto color = graph.ImportTexture("ViewportColor", {
			.extent = viewport.GetExtent(), .format = DXGI_FORMAT_R8G8B8A8_UNORM
		}, {
			.texture = viewport.GetColorTexture(),
			.renderTargetView = viewport.GetColorRenderTargetView()
		});
		const auto depth = graph.ImportTexture("ViewportDepth", {
			.extent = viewport.GetExtent(), .format = DXGI_FORMAT_D32_FLOAT
		}, {
			.texture = viewport.GetDepthTexture(),
			.writableDepthStencilView = viewport.GetDepthStencilView(),
			.readOnlyDepthStencilView = viewport.GetReadOnlyDepthStencilView()
		});
		return { color, depth };
	}

	// Empty mesh lists still clear attachments before the independently drawn grid.
	void AddMeshPass(RenderGraph& graph, RenderGraphTextureHandle color,
		RenderGraphTextureHandle depth, const VertexColorMeshPass& pass,
		const MeshManager& meshes, MeshPassCommands commands)
	{
		graph.AddPass("SceneMeshes", [=](GraphPassBuilder& builder)
		{
			builder.SetColorTarget(0, color, RenderGraphLoadOp::Clear,
				Color4{ 0.0f, 0.0f, 1.0f, 1.0f });
			builder.SetDepthTarget(depth, RenderGraphDepthAccess::ReadWrite,
				RenderGraphLoadOp::Clear);
		}, [&pass, &meshes, commands = std::move(commands)]
		(ID3D11DeviceContext& context)
		{
			if (!pass.ExecuteCommands(&context, commands, meshes))
				throw std::runtime_error("the scene mesh pass failed");
		});
	}

	// Uses a uniform foreground triangle to test the grid's projected fragment depth.
	MeshHandle CreateGreenTriangle(MeshManager& meshes, float z)
	{
		StaticMeshData data;
		data.vertices_.resize(3);
		data.vertices_[0].position = { -0.75f, -0.75f, z };
		data.vertices_[1].position = { 0.0f, 0.75f, z };
		data.vertices_[2].position = { 0.75f, -0.75f, z };
		for (NormalVertex& vertex : data.vertices_)
			vertex.color = { 0.0f, 1.0f, 0.0f, 1.0f };
		data.indices_ = { 0, 1, 2 };
		return meshes.CreateMesh(data);
	}

	// Both RGBA8 and D32_FLOAT center pixels occupy four bytes for readback.
	template<typename Pixel>
	std::optional<Pixel> ReadCenterPixel(ID3D11Texture2D* texture)
	{
		static_assert(sizeof(Pixel) == 4);
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
			(description.Width / 2) * sizeof(Pixel);
		Pixel result{};
		std::memcpy(&result, source, sizeof(Pixel));
		device.context->Unmap(staging.Get(), 0);
		return result;
	}

	// Accepts the one-byte rounding difference permitted by UNORM blending.
	bool HasColor(const std::array<std::uint8_t, 4>& pixel,
		int red, int green, int blue)
	{
		return std::abs(static_cast<int>(pixel[0]) - red) <= 1 &&
			std::abs(static_cast<int>(pixel[1]) - green) <= 1 &&
			std::abs(static_cast<int>(pixel[2]) - blue) <= 1;
	}
} // Anonymous namespace

GILGAMESH_TEST("Editor.Render.EditorGridPass", "RejectsInvalidInputsWithoutAddingPasses")
{
	const TestDevice& device = GetTestDevice();
	ShaderManager shaders;
	EditorGridPass pass;
	GILGAMESH_CHECK_MESSAGE(!pass.Initialize(nullptr, shaders) &&
		!pass.IsInitialized(), "a null device must not initialize a grid pass");
	if (!device.device) return;
	GILGAMESH_CHECK_MESSAGE(!pass.Initialize(device.device.Get(), shaders),
		"initialization must reject an uninitialized shader manager");
	EditorViewport viewport;
	if (!InitializePass(testContext, pass, shaders, viewport)) return;
	RenderGraph graph;
	const auto [color, depth] = ImportViewport(graph, viewport);
	RenderView view = MakeGridView(viewport.GetExtent());
	EditorConstants settings = MakeGridSettings();
	settings.gridSpacing = 0.0f;
	GILGAMESH_CHECK_MESSAGE(!pass.AddToGraph(graph, color, depth, view, settings),
		"zero grid spacing must be rejected before registering GPU work");
	settings = MakeGridSettings();
	settings.gridWidth = std::numeric_limits<float>::quiet_NaN();
	GILGAMESH_CHECK_MESSAGE(!pass.AddToGraph(graph, color, depth, view, settings),
		"non-finite grid settings must not reach the shader");
	settings = MakeGridSettings();
	view.projection(0, 0) = 0.0f;
	GILGAMESH_CHECK_MESSAGE(!pass.AddToGraph(graph, color, depth, view, settings) &&
		graph.GetPassCount() == 0,
		"invalid settings or a singular camera must leave graph declarations unchanged");
}

GILGAMESH_TEST("Editor.Render.EditorGridPass", "BlendsOverEmptyWorldAndPreservesSceneDepth")
{
	EditorGridPass grid;
	ShaderManager shaders;
	EditorViewport viewport;
	if (!InitializePass(testContext, grid, shaders, viewport)) return;
	const TestDevice& device = GetTestDevice();
	VertexColorMeshPass meshPass;
	MeshManager meshes;
	GILGAMESH_CHECK_MESSAGE(meshes.Initialize(device.device.Get()) &&
		meshPass.Initialize(device.device.Get(), shaders),
		"the empty scene must use the normal mesh pass");

	// No active scene yields an empty snapshot while the editor grid stays visible.
	World world;
	auto extractor = std::make_unique<RenderExtractionSystem>();
	RenderExtractionSystem& observer = *extractor;
	extractor->SetEnabled(true);
	world.GetExtractionSystems().AddSystem(std::move(extractor));
	world.PreparePresentation(0.0f);
	auto frame = observer.TakeFrame();
	GILGAMESH_CHECK_MESSAGE(frame && frame->staticMeshes.empty(),
		"a world without an active scene must still publish an empty render snapshot");
	if (!frame) return;
	const RenderView view = MakeGridView(viewport.GetExtent());
	auto commands = meshPass.PrepareCommands(*frame, view, meshes);
	if (!commands) return;
	RenderGraph graph;
	const auto [color, depth] = ImportViewport(graph, viewport);
	AddMeshPass(graph, color, depth, meshPass, meshes, std::move(*commands));
	GILGAMESH_CHECK_MESSAGE(grid.AddToGraph(graph, color, depth, view, MakeGridSettings()),
		"the grid overlay must be registered after scene meshes");
	GILGAMESH_CHECK_MESSAGE(graph.Compile(device.device.Get()) &&
		graph.Execute(device.context.Get()),
		"writable scene depth and read-only grid depth must compile and execute together");
	const auto pixel = ReadCenterPixel<std::array<std::uint8_t, 4>>(viewport.GetColorTexture());
	const auto retainedDepth = ReadCenterPixel<float>(viewport.GetDepthTexture());
	GILGAMESH_CHECK_MESSAGE(pixel && HasColor(*pixel, 128, 0, 128),
		"a half-opacity red grid must blend over the blue empty-scene clear");
	GILGAMESH_CHECK_MESSAGE(retainedDepth && *retainedDepth == 1.0f,
		"the grid must retain scene depth instead of writing its own projected depth");

	// The overlay must replace mesh draw state and bind its own procedural shaders.
	Microsoft::WRL::ComPtr<ID3D11DepthStencilState> depthState;
	Microsoft::WRL::ComPtr<ID3D11BlendState> blendState;
	Microsoft::WRL::ComPtr<ID3D11RasterizerState> rasterizer;
	Microsoft::WRL::ComPtr<ID3D11InputLayout> layout;
	device.context->OMGetDepthStencilState(depthState.GetAddressOf(), nullptr);
	device.context->OMGetBlendState(blendState.GetAddressOf(), nullptr, nullptr);
	device.context->RSGetState(rasterizer.GetAddressOf());
	device.context->IAGetInputLayout(layout.GetAddressOf());
	GILGAMESH_CHECK_MESSAGE(depthState && blendState && rasterizer && !layout,
		"a vertex-ID overlay must supply complete fixed state without a mesh input layout");
	if (depthState && blendState && rasterizer)
	{
		D3D11_DEPTH_STENCIL_DESC depthDescription{};
		D3D11_BLEND_DESC blendDescription{};
		D3D11_RASTERIZER_DESC rasterDescription{};
		depthState->GetDesc(&depthDescription);
		blendState->GetDesc(&blendDescription);
		rasterizer->GetDesc(&rasterDescription);
		GILGAMESH_CHECK_MESSAGE(depthDescription.DepthEnable &&
			depthDescription.DepthWriteMask == D3D11_DEPTH_WRITE_MASK_ZERO &&
			depthDescription.DepthFunc == D3D11_COMPARISON_LESS_EQUAL &&
			blendDescription.RenderTarget[0].BlendEnable &&
			rasterDescription.CullMode == D3D11_CULL_NONE,
			"grid drawing must use read-only LESS_EQUAL depth, alpha blending, and no culling");
	}
	device.context->ClearState();
}

GILGAMESH_TEST("Editor.Render.EditorGridPass", "UsesProjectedGridDepthToOccludeAgainstMeshes")
{
	EditorGridPass grid;
	ShaderManager shaders;
	EditorViewport viewport;
	if (!InitializePass(testContext, grid, shaders, viewport)) return;
	const TestDevice& device = GetTestDevice();
	VertexColorMeshPass meshPass;
	MeshManager meshes;
	GILGAMESH_CHECK_MESSAGE(meshes.Initialize(device.device.Get()) &&
		meshPass.Initialize(device.device.Get(), shaders),
		"the occluding geometry must use the real mesh pass");
	const RenderView view = MakeGridView(viewport.GetExtent());
	for (float meshZ : { -0.25f, 0.25f })
	{
		// A foreground triangle hides the grid; a background triangle receives the blend.
		const MeshHandle mesh = CreateGreenTriangle(meshes, meshZ);
		RenderFrameData frame;
		frame.staticMeshes.push_back({ mesh, Matrix4::Identity() });
		auto commands = meshPass.PrepareCommands(frame, view, meshes);
		if (!commands) return;
		RenderGraph graph;
		const auto [color, depth] = ImportViewport(graph, viewport);
		AddMeshPass(graph, color, depth, meshPass, meshes, std::move(*commands));
		GILGAMESH_CHECK_MESSAGE(grid.AddToGraph(graph, color, depth, view, MakeGridSettings()) &&
			graph.Compile(device.device.Get()) && graph.Execute(device.context.Get()),
			"the mesh and grid passes must execute with their shared depth texture");
		const auto pixel = ReadCenterPixel<std::array<std::uint8_t, 4>>(viewport.GetColorTexture());
		const auto retainedDepth = ReadCenterPixel<float>(viewport.GetDepthTexture());
		const bool foreground = meshZ < 0.0f;
		GILGAMESH_CHECK_MESSAGE(pixel && (foreground ? HasColor(*pixel, 0, 255, 0) :
			HasColor(*pixel, 128, 128, 0)),
			"grid fragment depth must hide behind foreground geometry and blend over background geometry");
		GILGAMESH_CHECK_MESSAGE(retainedDepth &&
			std::abs(*retainedDepth - (meshZ + 0.5f)) < 1e-5f,
			"both visible and occluded grid fragments must leave mesh depth intact");
		device.context->ClearState();
	}
}

GILGAMESH_TEST("Editor.Render.EditorGridPass", "KeepsQueuedViewsAndSettingsIndependent")
{
	EditorGridPass grid;
	ShaderManager shaders;
	EditorViewport firstViewport;
	if (!InitializePass(testContext, grid, shaders, firstViewport)) return;
	const TestDevice& device = GetTestDevice();
	EditorViewport secondViewport;
	secondViewport.Initialize(device.device.Get());
	secondViewport.RequestResize({ 32, 32 });
	GILGAMESH_CHECK_MESSAGE(secondViewport.ApplyPendingResize(),
		"the second view must have its own differently sized viewport");
	RenderGraph graph;
	const auto [firstColor, firstDepth] = ImportViewport(graph, firstViewport);
	const auto [secondColor, secondDepth] = ImportViewport(graph, secondViewport);
	for (const auto& targets : { std::pair{ firstColor, firstDepth },
		std::pair{ secondColor, secondDepth } })
	{
		graph.AddPass("ClearView", [=](GraphPassBuilder& builder)
		{
			builder.SetColorTarget(0, targets.first, RenderGraphLoadOp::Clear,
				Color4{ 0.0f, 0.0f, 1.0f, 1.0f });
			builder.SetDepthTarget(targets.second, RenderGraphDepthAccess::ReadWrite,
				RenderGraphLoadOp::Clear);
		}, [](ID3D11DeviceContext&) {});
	}
	RenderView view = MakeGridView(firstViewport.GetExtent());
	EditorConstants settings = MakeGridSettings();
	GILGAMESH_CHECK_MESSAGE(grid.AddToGraph(graph, firstColor, firstDepth, view, settings),
		"the first view must retain its own camera and red settings");
	view = MakeGridView(secondViewport.GetExtent());
	settings.gridColor = { 0.0f, 1.0f, 0.0f, 1.0f };
	GILGAMESH_CHECK_MESSAGE(grid.AddToGraph(graph, secondColor, secondDepth, view, settings),
		"the second view must retain its own camera and green settings");
	view = {};
	settings = {};
	GILGAMESH_CHECK_MESSAGE(graph.Compile(device.device.Get()) &&
		graph.Execute(device.context.Get()),
		"queued constants must outlive and ignore changes to their caller's values");
	const auto first = ReadCenterPixel<std::array<std::uint8_t, 4>>(firstViewport.GetColorTexture());
	const auto second = ReadCenterPixel<std::array<std::uint8_t, 4>>(secondViewport.GetColorTexture());
	GILGAMESH_CHECK_MESSAGE(first && HasColor(*first, 128, 0, 128) &&
		second && HasColor(*second, 0, 255, 0),
		"execution-time uploads must preserve independent views instead of reusing the last prepared constants");
	device.context->ClearState();
}

GILGAMESH_TEST("Editor.Render.EditorGridPass", "RecreatesMatchingReadOnlyDepthViewsOnResize")
{
	EditorGridPass grid;
	ShaderManager shaders;
	EditorViewport viewport;
	if (!InitializePass(testContext, grid, shaders, viewport)) return;
	const TestDevice& device = GetTestDevice();
	Microsoft::WRL::ComPtr<ID3D11DepthStencilView> oldReadOnly = viewport.GetReadOnlyDepthStencilView();
	Microsoft::WRL::ComPtr<ID3D11Texture2D> oldTexture = viewport.GetDepthTexture();
	viewport.RequestResize({ 96, 48 });
	GILGAMESH_CHECK_MESSAGE(viewport.ApplyPendingResize() && viewport.IsReady() &&
		viewport.GetReadOnlyDepthStencilView() != oldReadOnly.Get() &&
		viewport.GetDepthTexture() != oldTexture.Get(),
		"resize must publish a replacement depth texture and both replacement views together");
	D3D11_DEPTH_STENCIL_VIEW_DESC writableDescription{};
	D3D11_DEPTH_STENCIL_VIEW_DESC readOnlyDescription{};
	viewport.GetDepthStencilView()->GetDesc(&writableDescription);
	viewport.GetReadOnlyDepthStencilView()->GetDesc(&readOnlyDescription);
	Microsoft::WRL::ComPtr<ID3D11Resource> writableResource;
	Microsoft::WRL::ComPtr<ID3D11Resource> readOnlyResource;
	viewport.GetDepthStencilView()->GetResource(writableResource.GetAddressOf());
	viewport.GetReadOnlyDepthStencilView()->GetResource(readOnlyResource.GetAddressOf());
	GILGAMESH_CHECK_MESSAGE(writableDescription.Flags == 0 &&
		readOnlyDescription.Flags == D3D11_DSV_READ_ONLY_DEPTH &&
		writableResource.Get() == readOnlyResource.Get() &&
		readOnlyResource.Get() == viewport.GetDepthTexture(),
		"read-only and writable views must reference the same resized depth texture");

	// Importing the resized views exercises the graph's native view validation.
	RenderGraph graph;
	const auto [color, depth] = ImportViewport(graph, viewport);
	graph.AddPass("ClearResizedView", [=](GraphPassBuilder& builder)
	{
		builder.SetColorTarget(0, color, RenderGraphLoadOp::Clear);
		builder.SetDepthTarget(depth, RenderGraphDepthAccess::ReadWrite,
			RenderGraphLoadOp::Clear);
	}, [](ID3D11DeviceContext&) {});
	GILGAMESH_CHECK_MESSAGE(grid.AddToGraph(graph, color, depth,
		MakeGridView(viewport.GetExtent()), MakeGridSettings()) &&
		graph.Compile(device.device.Get()) && graph.Execute(device.context.Get()),
		"a resized viewport must still support writable mesh and read-only grid attachments");
	graph.Reset();
	device.context->ClearState();
	viewport.Shutdown();
	GILGAMESH_CHECK_MESSAGE(!viewport.IsReady() &&
		!viewport.GetDepthStencilView() && !viewport.GetReadOnlyDepthStencilView(),
		"shutdown must release both depth views after graph declarations are discarded");
}
