#include "Editor/EditorViewport/EditorViewport.h"
#include "Editor/Render/EditorViewportPasses.h"
#include "Editor/Render/EditorViewportRenderer.h"
#include "Engine/Render/Mesh/MeshManager.h"
#include "Engine/Render/Passes/VertexColorMeshPass.h"
#include "Engine/Render/Passes/ViewportClearPass.h"
#include "Engine/Render/Renderer/RenderFrameData.h"
#include "Engine/Render/Renderer/RenderView.h"
#include "Engine/Render/Shader/ShaderManager.h"
#include "Tests/TestFramework.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <vector>

#include <Windows.h>
#include <wrl/client.h>

namespace
{
	// Keeps headless renderer services alive longer than each tested pipeline.
	struct RenderFixture
	{
		Microsoft::WRL::ComPtr<ID3D11Device> device;
		Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
		ShaderManager shaders;
		MeshManager meshes;
		EditorViewport viewport;
	};

	// Finds test-owned shader outputs without relying on the working directory.
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

	// Uses real resources and WARP so module tests do not need a native window.
	bool InitializeFixture(Gilgamesh::Testing::TestContext& testContext,
		RenderFixture& fixture)
	{
		const bool ready = SUCCEEDED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP,
			nullptr, 0, nullptr, 0, D3D11_SDK_VERSION,
			fixture.device.GetAddressOf(), nullptr, fixture.context.GetAddressOf())) &&
			fixture.shaders.Initialize(fixture.device.Get(), GetShaderDirectory()) &&
			fixture.meshes.Initialize(fixture.device.Get());
		GILGAMESH_CHECK_MESSAGE(ready,
			"viewport rendering must initialize actual WARP services");
		if (!ready) return false;
		fixture.viewport.Initialize(fixture.device.Get());
		fixture.viewport.RequestResize({ 64, 64 });
		const bool resized = fixture.viewport.ApplyPendingResize();
		GILGAMESH_CHECK_MESSAGE(resized,
			"the module fixture must provide complete viewport attachments");
		return resized;
	}

	// Aligns the center pixel to the XY grid at projected depth 0.5.
	RenderView MakeView(Extent2D extent)
	{
		RenderView view{ .cameraWorldPos = { 0.0f, 0.0f, -1.0f }, .extent = extent };
		view.view(3, 0) = 1.0f / static_cast<float>(extent.width);
		view.view(3, 1) = -1.0f / static_cast<float>(extent.height);
		view.view(3, 2) = 0.5f;
		return view;
	}

	// Geometry behind the grid plane must still cover the earlier background pass.
	MeshHandle CreateGreenTriangle(MeshManager& meshes)
	{
		StaticMeshData data;
		data.vertices_.resize(3);
		data.vertices_[0].position = { -0.75f, -0.75f, 0.25f };
		data.vertices_[1].position = { 0.0f, 0.75f, 0.25f };
		data.vertices_[2].position = { 0.75f, -0.75f, 0.25f };
		for (NormalVertex& vertex : data.vertices_)
			vertex.color = { 0.0f, 1.0f, 0.0f, 1.0f };
		data.indices_ = { 0, 1, 2 };
		return meshes.CreateMesh(data);
	}

	// Reads actual output after the module has released its graph declarations.
	std::optional<std::array<std::uint8_t, 4>> ReadCenterPixel(
		const RenderFixture& fixture)
	{
		D3D11_TEXTURE2D_DESC description{};
		fixture.viewport.GetColorTexture()->GetDesc(&description);
		description.Usage = D3D11_USAGE_STAGING;
		description.BindFlags = 0;
		description.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
		description.MiscFlags = 0;
		Microsoft::WRL::ComPtr<ID3D11Texture2D> staging;
		if (FAILED(fixture.device->CreateTexture2D(
			&description, nullptr, staging.GetAddressOf()))) return std::nullopt;
		fixture.context->CopyResource(staging.Get(), fixture.viewport.GetColorTexture());
		D3D11_MAPPED_SUBRESOURCE mapped{};
		if (FAILED(fixture.context->Map(
			staging.Get(), 0, D3D11_MAP_READ, 0, &mapped))) return std::nullopt;
		std::array<std::uint8_t, 4> pixel{};
		const auto* source = static_cast<const std::uint8_t*>(mapped.pData) +
			(description.Height / 2) * mapped.RowPitch +
			(description.Width / 2) * pixel.size();
		std::memcpy(pixel.data(), source, pixel.size());
		fixture.context->Unmap(staging.Get(), 0);
		return pixel;
	}

	// UNORM blending permits a one-byte difference in the final color conversion.
	bool HasColor(const std::array<std::uint8_t, 4>& pixel,
		int red, int green, int blue)
	{
		return std::abs(static_cast<int>(pixel[0]) - red) <= 1 &&
			std::abs(static_cast<int>(pixel[1]) - green) <= 1 &&
			std::abs(static_cast<int>(pixel[2]) - blue) <= 1;
	}

	// Observes initialization, callback lifetime, and configurable failure boundaries.
	struct PassProbe
	{
		int initializations = 0;
		int executions = 0;
		int destructions = 0;
		bool rejectInitialization = false;
		bool rejectRegistration = false;
		bool throwRegistration = false;
		bool throwExecution = false;
		std::weak_ptr<int> callbackLifetime; // Expired when graph Reset releases captures.
		std::function<void()> initializeAction; // Optional nested call during initialization.
		std::function<void()> executeAction; // Optional nested call during submission.
	};

	// Supplies a real graph callback without depending on a particular built-in pass.
	class ProbePass final : public IRenderPass
	{
	public:
		explicit ProbePass(PassProbe& probe) : probe_(probe) {}
		~ProbePass() override { ++probe_.destructions; }

		bool Initialize(ID3D11Device*, ShaderManager&) override
		{
			++probe_.initializations;
			if (probe_.initializeAction) probe_.initializeAction();
			return !probe_.rejectInitialization;
		}

		bool AddToGraph(RenderGraph& graph, const RenderPassContext& context) const override
		{
			// An external weak observer proves every return path releases the callback.
			auto lifetime = std::make_shared<int>(0);
			probe_.callbackLifetime = lifetime;
			graph.AddPass("Probe", [color = context.color](GraphPassBuilder& builder)
			{
				builder.SetColorTarget(0, color, RenderGraphLoadOp::Load);
			}, [lifetime = std::move(lifetime), probe = &probe_](ID3D11DeviceContext&)
			{
				++probe->executions;
				if (probe->executeAction) probe->executeAction();
				if (probe->throwExecution) throw std::runtime_error("probe execution failed");
			});
			if (probe_.throwRegistration) throw std::runtime_error("probe registration failed");
			return !probe_.rejectRegistration;
		}

	private:
		PassProbe& probe_; // Borrowed test state outlives this pass and all its callbacks.
	};

	// Keeps a deterministic clear while testing an arbitrary interface implementation.
	std::vector<std::unique_ptr<IRenderPass>> MakeProbePipeline(PassProbe& probe)
	{
		std::vector<std::unique_ptr<IRenderPass>> passes;
		passes.push_back(std::make_unique<ViewportClearPass>());
		passes.push_back(std::make_unique<ProbePass>(probe));
		return passes;
	}
} // Anonymous namespace

GILGAMESH_TEST("Editor.Render.EditorViewportRenderer", "ExecutesFactoryPipelineAndReloadsResizedTargets")
{
	RenderFixture fixture;
	if (!InitializeFixture(testContext, fixture)) return;
	EditorViewportRenderer renderer;
	GILGAMESH_CHECK_MESSAGE(renderer.Initialize(fixture.device.Get(), fixture.context.Get(),
		fixture.shaders, fixture.meshes, CreateEditorViewportPasses()),
		"the configured editor pass list must initialize through the common interface");
	RenderFrameData frame;
	frame.staticMeshes.push_back({ CreateGreenTriangle(fixture.meshes), Matrix4::Identity() });
	RenderView view = MakeView(fixture.viewport.GetExtent());
	GILGAMESH_CHECK_MESSAGE(renderer.Render(fixture.viewport, frame, view),
		"the viewport module must execute the configured list synchronously");
	const auto meshPixel = ReadCenterPixel(fixture);
	GILGAMESH_CHECK_MESSAGE(meshPixel && HasColor(*meshPixel, 0, 255, 0),
		"the factory must draw opaque meshes after the background grid");

	// A new empty snapshot replaces the mesh color and retains the grid background.
	frame.staticMeshes.clear();
	GILGAMESH_CHECK_MESSAGE(renderer.Render(fixture.viewport, frame, view),
		"an empty frame must still execute clear and grid passes");
	const auto gridPixel = ReadCenterPixel(fixture);
	GILGAMESH_CHECK_MESSAGE(gridPixel && HasColor(*gridPixel, 84, 90, 101),
		"the empty mesh pass must load the factory's blended grid background");

	// Render returns before callers resize, so the next submission imports new targets.
	fixture.viewport.RequestResize({ 96, 48 });
	GILGAMESH_CHECK_MESSAGE(fixture.viewport.ApplyPendingResize(),
		"the viewport must be resizable immediately after a completed submission");
	view = MakeView(fixture.viewport.GetExtent());
	GILGAMESH_CHECK_MESSAGE(renderer.Render(fixture.viewport, frame, view),
		"the same pipeline must render newly imported viewport attachments");
	const auto resizedPixel = ReadCenterPixel(fixture);
	GILGAMESH_CHECK_MESSAGE(resizedPixel && HasColor(*resizedPixel, 84, 90, 101),
		"resizing must retain camera preparation and pass ordering");
}

GILGAMESH_TEST("Editor.Render.EditorViewportRenderer", "SupportsAConfiguredPipelineWithoutTheGrid")
{
	RenderFixture fixture;
	if (!InitializeFixture(testContext, fixture)) return;
	std::vector<std::unique_ptr<IRenderPass>> passes;
	passes.push_back(std::make_unique<ViewportClearPass>(Color4{ 0.0f, 0.0f, 1.0f, 1.0f }));
	passes.push_back(std::make_unique<VertexColorMeshPass>());
	EditorViewportRenderer renderer;
	GILGAMESH_CHECK_MESSAGE(renderer.Initialize(fixture.device.Get(), fixture.context.Get(),
		fixture.shaders, fixture.meshes, std::move(passes)),
		"pass selection must be supplied by the caller rather than hardcoded in the module");
	RenderFrameData frame;
	const RenderView view = MakeView(fixture.viewport.GetExtent());
	GILGAMESH_CHECK_MESSAGE(renderer.Render(fixture.viewport, frame, view),
		"Clear -> Mesh must work without a grid pass");
	const auto clearPixel = ReadCenterPixel(fixture);
	GILGAMESH_CHECK_MESSAGE(clearPixel && HasColor(*clearPixel, 0, 0, 255),
		"omitting the grid must retain the independent viewport clear");
	frame.staticMeshes.push_back({ CreateGreenTriangle(fixture.meshes), Matrix4::Identity() });
	GILGAMESH_CHECK_MESSAGE(renderer.Render(fixture.viewport, frame, view),
		"a customized pipeline must still render the extracted mesh list");
	const auto meshPixel = ReadCenterPixel(fixture);
	GILGAMESH_CHECK_MESSAGE(meshPixel && HasColor(*meshPixel, 0, 255, 0),
		"the module must submit mesh commands without knowing their concrete pass type");
}

GILGAMESH_TEST("Editor.Render.EditorViewportRenderer", "ReleasesCallbacksAfterSuccessAndAllSubmissionFailures")
{
	RenderFixture fixture;
	if (!InitializeFixture(testContext, fixture)) return;
	PassProbe probe;
	EditorViewportRenderer renderer;
	const bool initialized = renderer.Initialize(fixture.device.Get(), fixture.context.Get(),
		fixture.shaders, fixture.meshes, MakeProbePipeline(probe));
	GILGAMESH_CHECK_MESSAGE(initialized,
		"an arbitrary interface implementation must initialize in the owned pass list");
	if (!initialized) return;
	RenderFrameData frame;
	const RenderView view = MakeView(fixture.viewport.GetExtent());
	GILGAMESH_CHECK_MESSAGE(renderer.Render(fixture.viewport, frame, view) &&
		probe.executions == 1 && probe.callbackLifetime.expired(),
		"successful submission must release graph-owned callbacks before returning");

	// Registration can fail after it has already added a callback to the graph.
	probe.rejectRegistration = true;
	GILGAMESH_CHECK_MESSAGE(!renderer.Render(fixture.viewport, frame, view) &&
		probe.executions == 1 && probe.callbackLifetime.expired(),
		"a rejected registration must release partial work without executing it");
	probe.rejectRegistration = false;
	probe.throwRegistration = true;
	GILGAMESH_CHECK_MESSAGE(!renderer.Render(fixture.viewport, frame, view) &&
		probe.executions == 1 && probe.callbackLifetime.expired(),
		"an exception during registration must release partial work");
	probe.throwRegistration = false;
	probe.throwExecution = true;
	GILGAMESH_CHECK_MESSAGE(!renderer.Render(fixture.viewport, frame, view) &&
		probe.executions == 2 && probe.callbackLifetime.expired(),
		"an exception during execution must release the failing callback");

	// The next successful frame executes exactly once instead of replaying old work.
	probe.throwExecution = false;
	GILGAMESH_CHECK_MESSAGE(renderer.Render(fixture.viewport, frame, view) &&
		probe.executions == 3 && probe.callbackLifetime.expired(),
		"a pipeline must recover after failed submission without retaining prior commands");
}

GILGAMESH_TEST("Editor.Render.EditorViewportRenderer", "PreservesLivePipelineOnRejectedReplacementAndRejectsReentry")
{
	RenderFixture fixture;
	if (!InitializeFixture(testContext, fixture)) return;
	PassProbe live;
	PassProbe rejected;
	PassProbe nested;
	EditorViewportRenderer renderer;
	RenderFrameData frame;
	const RenderView view = MakeView(fixture.viewport.GetExtent());
	bool nestedInitialization = true;
	live.initializeAction = [&]
	{
		nestedInitialization = renderer.Initialize(fixture.device.Get(), fixture.context.Get(),
			fixture.shaders, fixture.meshes, MakeProbePipeline(nested));
	};
	GILGAMESH_CHECK_MESSAGE(renderer.Initialize(fixture.device.Get(), fixture.context.Get(),
		fixture.shaders, fixture.meshes, MakeProbePipeline(live)) &&
		!nestedInitialization && nested.initializations == 0,
		"an initialization callback must not replace the pipeline being initialized");
	live.initializeAction = {};

	// Incoming initialization failure must leave the existing passes alive and usable.
	rejected.rejectInitialization = true;
	GILGAMESH_CHECK_MESSAGE(!renderer.Initialize(fixture.device.Get(), fixture.context.Get(),
		fixture.shaders, fixture.meshes, MakeProbePipeline(rejected)) &&
		renderer.IsInitialized() && rejected.destructions == 1 && live.destructions == 0,
		"a failed replacement must retire only the rejected incoming pass list");
	bool nestedRender = true;
	bool nestedReplacement = true;
	live.executeAction = [&]
	{
		nestedRender = renderer.Render(fixture.viewport, frame, view);
		nestedReplacement = renderer.Initialize(fixture.device.Get(), fixture.context.Get(),
			fixture.shaders, fixture.meshes, MakeProbePipeline(nested));
	};
	GILGAMESH_CHECK_MESSAGE(renderer.Render(fixture.viewport, frame, view) &&
		!nestedRender && !nestedReplacement && live.executions == 1 &&
		nested.initializations == 0 && live.callbackLifetime.expired(),
		"callbacks must not reset an executing graph or destroy the active pass list");
	live.executeAction = {};
	GILGAMESH_CHECK_MESSAGE(renderer.Render(fixture.viewport, frame, view) && live.executions == 2,
		"rejected nested calls must leave later ordinary submissions available");
}
