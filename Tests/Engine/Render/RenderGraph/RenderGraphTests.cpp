#include "Engine/Render/RenderGraph/RenderGraph.h"
#include "Tests/TestFramework.h"

#include <memory>
#include <stdexcept>

namespace
{
	RenderGraphTextureDesc MakeTextureDescription(
		DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM)
	{
		RenderGraphTextureDesc description;
		description.extent = { 128, 128 };
		description.format = format;
		return description;
	}
} // Anonymous namespace

GILGAMESH_TEST(
	"Engine.Render.RenderGraph",
	"CreatesGraphOwnedTextureHandles")
{
	RenderGraph firstGraph;
	RenderGraph secondGraph;

	const RenderGraphTextureHandle invalidHandle;
	const RenderGraphTextureHandle color = firstGraph.CreateTexture(
		"SceneColor",
		MakeTextureDescription());
	const RenderGraphTextureHandle imported = firstGraph.ImportTexture(
		"Viewport",
		MakeTextureDescription(),
		ImportedRenderGraphTexture{});

	GILGAMESH_CHECK_MESSAGE(
		!invalidHandle && !firstGraph.IsValid(invalidHandle),
		"a default texture handle must be invalid");
	GILGAMESH_CHECK_MESSAGE(
		color && imported && color != imported,
		"each texture declaration must receive a distinct handle");
	GILGAMESH_CHECK_MESSAGE(
		firstGraph.IsValid(color) && firstGraph.IsValid(imported),
		"the creating graph must recognize its texture handles");
	GILGAMESH_CHECK_MESSAGE(
		!secondGraph.IsValid(color) && !secondGraph.IsValid(imported),
		"a graph must reject texture handles owned by another graph");
	GILGAMESH_CHECK_MESSAGE(
		firstGraph.GetTextureCount() == 2,
		"logical texture declarations must be retained without a GPU");
}

GILGAMESH_TEST(
	"Engine.Render.RenderGraph",
	"ResetInvalidatesHandlesAndDeclarations")
{
	RenderGraph graph;
	const RenderGraphTextureHandle oldTexture = graph.CreateTexture(
		"OldTexture",
		MakeTextureDescription());

	graph.AddPass(
		"OldPass",
		[](GraphPassBuilder&) {},
		[](ID3D11DeviceContext&) {});

	graph.Reset();

	GILGAMESH_CHECK_MESSAGE(
		oldTexture && !graph.IsValid(oldTexture),
		"Reset must invalidate handles without changing their syntax");
	GILGAMESH_CHECK_MESSAGE(
		graph.GetTextureCount() == 0 && graph.GetPassCount() == 0,
		"Reset must discard every resource and pass declaration");

	const RenderGraphTextureHandle newTexture = graph.CreateTexture(
		"NewTexture",
		MakeTextureDescription());

	GILGAMESH_CHECK_MESSAGE(
		graph.IsValid(newTexture) && newTexture != oldTexture,
		"a reused resource index must receive a fresh generation");
	GILGAMESH_CHECK_MESSAGE(
		!graph.IsValid(oldTexture),
		"creating a new resource must not revive a stale handle");
}

GILGAMESH_TEST(
	"Engine.Render.RenderGraph",
	"StoresPassDeclarationsWithoutExecutingThem")
{
	RenderGraph graph;
	const RenderGraphTextureHandle color = graph.CreateTexture(
		"Color",
		MakeTextureDescription());
	const RenderGraphTextureHandle depth = graph.CreateTexture(
		"Depth",
		MakeTextureDescription(DXGI_FORMAT_D32_FLOAT));
	const RenderGraphTextureHandle vertexInput = graph.CreateTexture(
		"VertexInput",
		MakeTextureDescription());
	const RenderGraphTextureHandle pixelInput = graph.CreateTexture(
		"PixelInput",
		MakeTextureDescription());

	int setupCalls = 0;
	int executeCalls = 0;

	graph.AddPass(
		"DeclaredPass",
		[&](GraphPassBuilder& builder)
		{
			++setupCalls;
			builder.SetColorTarget(
				0,
				color,
				RenderGraphLoadOp::Clear,
				Color4(0.1f, 0.2f, 0.3f));
			builder.SetDepthTarget(
				depth,
				RenderGraphDepthAccess::ReadWrite,
				RenderGraphLoadOp::Clear);
			builder.ReadVertexTexture(vertexInput, 0);
			builder.ReadPixelTexture(pixelInput, 1);
		},
		[&](ID3D11DeviceContext&)
		{
			++executeCalls;
		});

	GILGAMESH_CHECK_MESSAGE(
		setupCalls == 1,
		"AddPass must invoke its setup callback exactly once");
	GILGAMESH_CHECK_MESSAGE(
		executeCalls == 0,
		"declaring a pass must not execute its rendering callback");
	GILGAMESH_CHECK_MESSAGE(
		graph.GetPassCount() == 1,
		"AddPass must retain one complete pass declaration");
}

GILGAMESH_TEST(
	"Engine.Render.RenderGraph",
	"OwnsExecuteCallbackUntilReset")
{
	RenderGraph graph;
	int setupCalls = 0;
	int executeCalls = 0;

	auto setupToken = std::make_shared<int>(1);
	auto executeToken = std::make_shared<int>(2);
	const std::weak_ptr<int> setupLifetime = setupToken;
	const std::weak_ptr<int> executeLifetime = executeToken;

	graph.AddPass(
		"Lifetime",
		[
			&setupCalls,
			token = std::move(setupToken)
		](GraphPassBuilder&)
		{
			++setupCalls;
		},
		[
			&executeCalls,
			token = std::move(executeToken)
		](ID3D11DeviceContext&)
		{
			++executeCalls;
		});

	GILGAMESH_CHECK_MESSAGE(
		setupCalls == 1 && setupLifetime.expired(),
		"the synchronous setup callback must not be retained");
	GILGAMESH_CHECK_MESSAGE(
		executeCalls == 0 && !executeLifetime.expired(),
		"the pass declaration must own its uncalled execute callback");

	graph.Reset();

	GILGAMESH_CHECK_MESSAGE(
		executeCalls == 0 && executeLifetime.expired(),
		"Reset must release execute callback captures without invoking them");
}

GILGAMESH_TEST(
	"Engine.Render.RenderGraph",
	"DoesNotKeepFailedPassSetup")
{
	RenderGraph graph;
	bool setupThrew = false;

	try
	{
		graph.AddPass(
			"BrokenPass",
			[](GraphPassBuilder&)
			{
				throw std::runtime_error("setup failed");
			},
			[](ID3D11DeviceContext&) {});
	}
	catch (const std::runtime_error&)
	{
		setupThrew = true;
	}

	GILGAMESH_CHECK_MESSAGE(
		setupThrew,
		"AddPass must propagate a setup failure to the caller");
	GILGAMESH_CHECK_MESSAGE(
		graph.GetPassCount() == 0,
		"a failed setup must not publish a partial pass declaration");
}

GILGAMESH_TEST(
	"Engine.Render.RenderGraph",
	"RejectsGraphMutationInsidePassSetup")
{
	RenderGraph graph;
	bool resourceCreationRejected = false;
	bool nestedPassRejected = false;

	graph.AddPass(
		"OuterPass",
		[&](GraphPassBuilder&)
		{
			try
			{
				(void)graph.CreateTexture(
					"LateTexture",
					MakeTextureDescription());
			}
			catch (const std::logic_error&)
			{
				resourceCreationRejected = true;
			}

			try
			{
				graph.AddPass(
					"NestedPass",
					[](GraphPassBuilder&) {},
					[](ID3D11DeviceContext&) {});
			}
			catch (const std::logic_error&)
			{
				nestedPassRejected = true;
			}
		},
		[](ID3D11DeviceContext&) {});

	GILGAMESH_CHECK_MESSAGE(
		resourceCreationRejected && graph.GetTextureCount() == 0,
		"pass setup must not mutate the graph resource list directly");
	GILGAMESH_CHECK_MESSAGE(
		nestedPassRejected && graph.GetPassCount() == 1,
		"pass setup must not register nested passes");
}
