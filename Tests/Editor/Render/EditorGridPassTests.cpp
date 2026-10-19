#include "Editor/EditorViewport/EditorViewport.h"
#include "Editor/Render/EditorGridPass.h"
#include "Engine/ECS/Framework/World.h"
#include "Engine/ECS/System/RenderExtractionSystem.h"
#include "Engine/Render/Mesh/MeshManager.h"
#include "Engine/Render/Passes/VertexColorMeshPass.h"
#include "Engine/Render/Passes/ViewportClearPass.h"
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

	// Initializes the real background-grid resources and viewport attachments.
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

	// Imports only writable depth; a background grid must not need a read-only view.
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
			.writableDepthStencilView = viewport.GetDepthStencilView()
		});
		return { color, depth };
	}

	// Exercises the same interface used by an owned viewport pass list.
	bool AddPass(RenderGraph& graph, RenderGraphTextureHandle color,
		RenderGraphTextureHandle depth, const IRenderPass& pass,
		const RenderFrameData& frame, const RenderView& view,
		const MeshManager& meshes)
	{
		return pass.AddToGraph(graph, {
			.color = color, .depth = depth, .frame = frame,
			.view = view, .meshes = meshes
		});
	}

	// Places geometry on either side of the grid plane with a uniform green color.
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
	RenderFrameData frame;
	MeshManager meshes;
	RenderView view = MakeGridView(viewport.GetExtent());
	EditorConstants settings = MakeGridSettings();
	settings.gridSpacing = 0.0f;
	GILGAMESH_CHECK_MESSAGE(!pass.SetSettings(settings),
		"zero grid spacing must not replace the grid's valid configuration");
	settings = MakeGridSettings();
	settings.gridWidth = std::numeric_limits<float>::quiet_NaN();
	GILGAMESH_CHECK_MESSAGE(!pass.SetSettings(settings),
		"non-finite grid settings must not replace the grid's valid configuration");
	settings = MakeGridSettings();
	GILGAMESH_CHECK_MESSAGE(pass.SetSettings(settings),
		"valid editor settings must be accepted before graph registration");
	view.projection(0, 0) = 0.0f;
	GILGAMESH_CHECK_MESSAGE(!AddPass(graph, color, depth, pass, frame, view, meshes) &&
		graph.GetPassCount() == 0,
		"a singular camera must leave graph declarations unchanged");
}

GILGAMESH_TEST("Editor.Render.EditorGridPass", "BlendsAfterClearWithoutBindingOrChangingDepth")
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
	ViewportClearPass clear{ Color4{ 0.0f, 0.0f, 1.0f, 1.0f } };
	GILGAMESH_CHECK_MESSAGE(clear.Initialize(device.device.Get(), shaders) &&
		grid.SetSettings(MakeGridSettings()),
		"the independent clear and grid passes must initialize");
	RenderGraph graph;
	const auto [color, depth] = ImportViewport(graph, viewport);
	GILGAMESH_CHECK_MESSAGE(AddPass(graph, color, depth, clear, *frame, view, meshes) &&
		AddPass(graph, color, depth, grid, *frame, view, meshes),
		"the grid must follow the independent viewport clear");
	GILGAMESH_CHECK_MESSAGE(graph.Compile(device.device.Get()) &&
		graph.Execute(device.context.Get()),
		"the clear and background-grid passes must compile and execute together");
	const auto pixel = ReadCenterPixel<std::array<std::uint8_t, 4>>(viewport.GetColorTexture());
	const auto retainedDepth = ReadCenterPixel<float>(viewport.GetDepthTexture());
	GILGAMESH_CHECK_MESSAGE(pixel && HasColor(*pixel, 128, 0, 128),
		"a half-opacity red grid must blend over the blue viewport clear");
	GILGAMESH_CHECK_MESSAGE(retainedDepth && *retainedDepth == 1.0f,
		"the grid must retain the clear depth value");

	// The background grid supplies its own state and needs no depth attachment.
	Microsoft::WRL::ComPtr<ID3D11DepthStencilState> depthState;
	Microsoft::WRL::ComPtr<ID3D11BlendState> blendState;
	Microsoft::WRL::ComPtr<ID3D11RasterizerState> rasterizer;
	Microsoft::WRL::ComPtr<ID3D11InputLayout> layout;
	device.context->OMGetDepthStencilState(depthState.GetAddressOf(), nullptr);
	device.context->OMGetBlendState(blendState.GetAddressOf(), nullptr, nullptr);
	device.context->RSGetState(rasterizer.GetAddressOf());
	device.context->IAGetInputLayout(layout.GetAddressOf());
	GILGAMESH_CHECK_MESSAGE(depthState && blendState && rasterizer && !layout,
		"a vertex-ID grid must supply complete fixed state without a mesh input layout");
	if (depthState && blendState && rasterizer)
	{
		D3D11_DEPTH_STENCIL_DESC depthDescription{};
		D3D11_BLEND_DESC blendDescription{};
		D3D11_RASTERIZER_DESC rasterDescription{};
		depthState->GetDesc(&depthDescription);
		blendState->GetDesc(&blendDescription);
		rasterizer->GetDesc(&rasterDescription);
		// D3D11 may normalize the unused write mask while depth testing is disabled.
		// The depth readback above checks that the grid actually preserves the texture.
		GILGAMESH_CHECK_MESSAGE(!depthDescription.DepthEnable &&
			blendDescription.RenderTarget[0].BlendEnable &&
			rasterDescription.CullMode == D3D11_CULL_NONE,
			"grid drawing must disable depth testing while using alpha blending and no culling");
	}

	// Even an empty mesh pass must load and preserve the preceding background.
	graph.Reset();
	const auto [reloadedColor, reloadedDepth] = ImportViewport(graph, viewport);
	GILGAMESH_CHECK_MESSAGE(
		AddPass(graph, reloadedColor, reloadedDepth, clear, *frame, view, meshes) &&
		AddPass(graph, reloadedColor, reloadedDepth, grid, *frame, view, meshes) &&
		AddPass(graph, reloadedColor, reloadedDepth, meshPass, *frame, view, meshes) &&
		graph.Compile(device.device.Get()) && graph.Execute(device.context.Get()),
		"Clear -> Grid -> Mesh must work for an empty extracted scene");
	const auto preserved = ReadCenterPixel<std::array<std::uint8_t, 4>>(
		viewport.GetColorTexture());
	GILGAMESH_CHECK_MESSAGE(preserved && HasColor(*preserved, 128, 0, 128),
		"an empty mesh pass must preserve the grid instead of clearing it again");
	device.context->ClearState();
}

GILGAMESH_TEST("Editor.Render.EditorGridPass", "OpaqueMeshesCoverGridOnBothSidesOfItsPlane")
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
		"the opaque geometry must use the real mesh pass");
	ViewportClearPass clear{ Color4{ 0.0f, 0.0f, 1.0f, 1.0f } };
	GILGAMESH_CHECK_MESSAGE(clear.Initialize(device.device.Get(), shaders) &&
		grid.SetSettings(MakeGridSettings()),
		"the independent viewport clear and grid must initialize");
	const RenderView view = MakeGridView(viewport.GetExtent());
	for (float meshZ : { -0.25f, 0.25f })
	{
		// Both triangles draw after the grid regardless of their side of its plane.
		const MeshHandle mesh = CreateGreenTriangle(meshes, meshZ);
		RenderFrameData frame;
		frame.staticMeshes.push_back({ mesh, Matrix4::Identity() });
		RenderGraph graph;
		const auto [color, depth] = ImportViewport(graph, viewport);
		GILGAMESH_CHECK_MESSAGE(AddPass(graph, color, depth, clear, frame, view, meshes) &&
			AddPass(graph, color, depth, grid, frame, view, meshes) &&
			AddPass(graph, color, depth, meshPass, frame, view, meshes) &&
			graph.Compile(device.device.Get()) && graph.Execute(device.context.Get()),
			"Clear -> Grid -> Mesh must execute through the common pass interface");
		const auto pixel = ReadCenterPixel<std::array<std::uint8_t, 4>>(viewport.GetColorTexture());
		const auto retainedDepth = ReadCenterPixel<float>(viewport.GetDepthTexture());
		GILGAMESH_CHECK_MESSAGE(pixel && HasColor(*pixel, 0, 255, 0),
			"both foreground and background opaque meshes must cover the background grid");
		GILGAMESH_CHECK_MESSAGE(retainedDepth &&
			std::abs(*retainedDepth - (meshZ + 0.5f)) < 1e-5f,
			"the grid must not interfere with either mesh's normal depth writes");
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
	RenderFrameData frame;
	MeshManager meshes;
	GILGAMESH_CHECK_MESSAGE(grid.SetSettings(settings) &&
		AddPass(graph, firstColor, firstDepth, grid, frame, view, meshes),
		"the first view must retain its own camera and red settings");
	view = MakeGridView(secondViewport.GetExtent());
	settings.gridColor = { 0.0f, 1.0f, 0.0f, 1.0f };
	GILGAMESH_CHECK_MESSAGE(grid.SetSettings(settings) &&
		AddPass(graph, secondColor, secondDepth, grid, frame, view, meshes),
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
	RenderFrameData frame;
	MeshManager meshes;
	GILGAMESH_CHECK_MESSAGE(grid.SetSettings(MakeGridSettings()) &&
		AddPass(graph, color, depth, grid, frame,
			MakeGridView(viewport.GetExtent()), meshes) &&
		graph.Compile(device.device.Get()) && graph.Execute(device.context.Get()),
		"a resized viewport must support its clear and background-grid attachments");
	graph.Reset();
	device.context->ClearState();
	viewport.Shutdown();
	GILGAMESH_CHECK_MESSAGE(!viewport.IsReady() &&
		!viewport.GetDepthStencilView() && !viewport.GetReadOnlyDepthStencilView(),
		"shutdown must release both depth views after graph declarations are discarded");
}
