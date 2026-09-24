#include "Engine/Render/RenderGraph/RenderGraph.h"
#include "Tests/TestFramework.h"

#include <memory>
#include <stdexcept>
#include <wrl/client.h>

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

// A unique transient producer may be scheduled before an earlier declaration.
GILGAMESH_TEST(
	"Engine.Render.RenderGraph",
	"CompileOrdersSingleWriterBeforeEarlierReader")
{
	RenderGraph graph;
	const auto texture = graph.CreateTexture(
		"SceneColor",
		MakeTextureDescription());

	graph.AddPass(
		"Reader",
		[&](GraphPassBuilder& builder)
		{
			builder.ReadPixelTexture(texture, 0);
		},
		[](ID3D11DeviceContext&) {});
	graph.AddPass(
		"Writer",
		[&](GraphPassBuilder& builder)
		{
			builder.SetColorTarget(0, texture, RenderGraphLoadOp::Clear);
		},
		[](ID3D11DeviceContext&) {});

	const bool compiled = graph.Compile();
	const auto order = graph.GetCompiledPassOrder();
	GILGAMESH_CHECK_MESSAGE(
		compiled && graph.IsCompiled(),
		"a reader registered before the only transient writer must compile");
	GILGAMESH_CHECK_MESSAGE(
		order.size() == 2 && order[0] == 1 && order[1] == 0,
		"the producer must run before its earlier-declared consumer");
}

// A Load attachment preserves the preceding writer's contents for later reads.
GILGAMESH_TEST(
	"Engine.Render.RenderGraph",
	"CompileOrdersClearLoadAndRead")
{
	RenderGraph graph;
	const auto texture = graph.CreateTexture(
		"SceneColor",
		MakeTextureDescription());

	graph.AddPass(
		"Scene",
		[&](GraphPassBuilder& builder)
		{
			builder.SetColorTarget(0, texture, RenderGraphLoadOp::Clear);
		},
		[](ID3D11DeviceContext&) {});
	graph.AddPass(
		"Grid",
		[&](GraphPassBuilder& builder)
		{
			builder.SetColorTarget(0, texture, RenderGraphLoadOp::Load);
		},
		[](ID3D11DeviceContext&) {});
	graph.AddPass(
		"Composite",
		[&](GraphPassBuilder& builder)
		{
			builder.ReadPixelTexture(texture, 0);
		},
		[](ID3D11DeviceContext&) {});

	const bool compiled = graph.Compile();
	const auto order = graph.GetCompiledPassOrder();
	GILGAMESH_CHECK_MESSAGE(
		compiled && graph.IsCompiled(),
		"Clear, Load, and shader read must form a valid access chain");
	GILGAMESH_CHECK_MESSAGE(
		order.size() == 3 && order[0] == 0 && order[1] == 1
			&& order[2] == 2,
		"Load must follow Clear and the final reader must follow Load");
}

// A read between two writes observes the first version, not the later one.
GILGAMESH_TEST(
	"Engine.Render.RenderGraph",
	"CompilePlacesReadBetweenSuccessiveWriters")
{
	RenderGraph graph;
	const auto texture = graph.CreateTexture(
		"SceneColor",
		MakeTextureDescription());

	graph.AddPass(
		"FirstWrite",
		[&](GraphPassBuilder& builder)
		{
			builder.SetColorTarget(0, texture, RenderGraphLoadOp::Clear);
		},
		[](ID3D11DeviceContext&) {});
	graph.AddPass(
		"ReadFirstVersion",
		[&](GraphPassBuilder& builder)
		{
			builder.ReadPixelTexture(texture, 0);
		},
		[](ID3D11DeviceContext&) {});
	graph.AddPass(
		"SecondWrite",
		[&](GraphPassBuilder& builder)
		{
			builder.SetColorTarget(0, texture, RenderGraphLoadOp::Clear);
		},
		[](ID3D11DeviceContext&) {});

	const bool compiled = graph.Compile();
	const auto order = graph.GetCompiledPassOrder();
	GILGAMESH_CHECK_MESSAGE(
		compiled && graph.IsCompiled(),
		"an intermediate read between two writes must compile");
	GILGAMESH_CHECK_MESSAGE(
		order.size() == 3 && order[0] == 0 && order[1] == 1
			&& order[2] == 2,
		"the intermediate reader must run before the second writer");
}

// An imported texture can be read before the graph's first writer.
GILGAMESH_TEST(
	"Engine.Render.RenderGraph",
	"CompilePreservesImportedInitialRead")
{
	Microsoft::WRL::ComPtr<ID3D11Device> device;
	Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
	const HRESULT createDeviceResult = D3D11CreateDevice(
		nullptr,
		D3D_DRIVER_TYPE_WARP,
		nullptr,
		0,
		nullptr,
		0,
		D3D11_SDK_VERSION,
		device.GetAddressOf(),
		nullptr,
		context.GetAddressOf());
	GILGAMESH_CHECK_MESSAGE(
		SUCCEEDED(createDeviceResult) && device,
		"the WARP device must be available for the imported texture test");
	if (FAILED(createDeviceResult) || !device) return;

	// Keep the native texture and both required views alive during compilation.
	D3D11_TEXTURE2D_DESC nativeDescription{};
	nativeDescription.Width = 128;
	nativeDescription.Height = 128;
	nativeDescription.MipLevels = 1;
	nativeDescription.ArraySize = 1;
	nativeDescription.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	nativeDescription.SampleDesc.Count = 1;
	nativeDescription.Usage = D3D11_USAGE_DEFAULT;
	nativeDescription.BindFlags =
		D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

	Microsoft::WRL::ComPtr<ID3D11Texture2D> nativeTexture;
	Microsoft::WRL::ComPtr<ID3D11RenderTargetView> renderTargetView;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> shaderResourceView;
	const HRESULT createTextureResult = device->CreateTexture2D(
		&nativeDescription,
		nullptr,
		nativeTexture.GetAddressOf());
	GILGAMESH_CHECK_MESSAGE(
		SUCCEEDED(createTextureResult) && nativeTexture,
		"the imported texture must be created");
	if (FAILED(createTextureResult) || !nativeTexture) return;

	const HRESULT createTargetResult = device->CreateRenderTargetView(
		nativeTexture.Get(),
		nullptr,
		renderTargetView.GetAddressOf());
	const HRESULT createViewResult = device->CreateShaderResourceView(
		nativeTexture.Get(),
		nullptr,
		shaderResourceView.GetAddressOf());
	GILGAMESH_CHECK_MESSAGE(
		SUCCEEDED(createTargetResult) && renderTargetView
			&& SUCCEEDED(createViewResult) && shaderResourceView,
		"the imported texture must have target and shader-read views");
	if (FAILED(createTargetResult) || !renderTargetView
		|| FAILED(createViewResult) || !shaderResourceView) return;

	RenderGraph graph;
	const auto texture = graph.ImportTexture(
		"ImportedColor",
		MakeTextureDescription(),
		ImportedRenderGraphTexture{
			nativeTexture.Get(),
			renderTargetView.Get(),
			nullptr,
			shaderResourceView.Get()
		});
	graph.AddPass(
		"ReadExternalContents",
		[&](GraphPassBuilder& builder)
		{
			builder.ReadPixelTexture(texture, 0);
		},
		[](ID3D11DeviceContext&) {});
	graph.AddPass(
		"ReplaceContents",
		[&](GraphPassBuilder& builder)
		{
			builder.SetColorTarget(0, texture, RenderGraphLoadOp::Clear);
		},
		[](ID3D11DeviceContext&) {});

	const bool compiled = graph.Compile();
	const auto order = graph.GetCompiledPassOrder();
	GILGAMESH_CHECK_MESSAGE(
		compiled && graph.IsCompiled(),
		"an imported texture may expose its initial contents to a reader");
	GILGAMESH_CHECK_MESSAGE(
		order.size() == 2 && order[0] == 0 && order[1] == 1,
		"the initial read must precede the graph's first imported writer");
}

// Two handles for one native texture would hide a read/write hazard.
GILGAMESH_TEST(
	"Engine.Render.RenderGraph",
	"CompileRejectsAliasedImportedTextures")
{
	Microsoft::WRL::ComPtr<ID3D11Device> device;
	Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
	const HRESULT createDeviceResult = D3D11CreateDevice(
		nullptr,
		D3D_DRIVER_TYPE_WARP,
		nullptr,
		0,
		nullptr,
		0,
		D3D11_SDK_VERSION,
		device.GetAddressOf(),
		nullptr,
		context.GetAddressOf());
	GILGAMESH_CHECK_MESSAGE(
		SUCCEEDED(createDeviceResult) && device,
		"the WARP device must be available for the import alias test");
	if (FAILED(createDeviceResult) || !device) return;

	D3D11_TEXTURE2D_DESC nativeDescription{};
	nativeDescription.Width = 128;
	nativeDescription.Height = 128;
	nativeDescription.MipLevels = 1;
	nativeDescription.ArraySize = 1;
	nativeDescription.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	nativeDescription.SampleDesc.Count = 1;
	nativeDescription.Usage = D3D11_USAGE_DEFAULT;
	nativeDescription.BindFlags = D3D11_BIND_RENDER_TARGET;
	Microsoft::WRL::ComPtr<ID3D11Texture2D> nativeTexture;
	const HRESULT createTextureResult = device->CreateTexture2D(
		&nativeDescription,
		nullptr,
		nativeTexture.GetAddressOf());
	GILGAMESH_CHECK_MESSAGE(
		SUCCEEDED(createTextureResult) && nativeTexture,
		"the native texture must be created for the import alias test");
	if (FAILED(createTextureResult) || !nativeTexture) return;

	RenderGraph graph;
	const ImportedRenderGraphTexture imported{ nativeTexture.Get() };
	(void)graph.ImportTexture(
		"FirstAlias",
		MakeTextureDescription(),
		imported);
	(void)graph.ImportTexture(
		"SecondAlias",
		MakeTextureDescription(),
		imported);

	GILGAMESH_CHECK_MESSAGE(
		!graph.Compile() && !graph.IsCompiled()
			&& graph.GetCompiledPassOrder().empty(),
		"one native texture must not be imported under two graph handles");
}

// Without imported contents, Load cannot initialize a transient texture.
GILGAMESH_TEST(
	"Engine.Render.RenderGraph",
	"CompileRejectsFirstTransientLoad")
{
	RenderGraph graph;
	const auto texture = graph.CreateTexture(
		"Uninitialized",
		MakeTextureDescription());

	graph.AddPass(
		"LoadWithoutProducer",
		[&](GraphPassBuilder& builder)
		{
			builder.SetColorTarget(0, texture, RenderGraphLoadOp::Load);
		},
		[](ID3D11DeviceContext&) {});

	GILGAMESH_CHECK_MESSAGE(
		!graph.Compile(),
		"the first access to a transient texture cannot be Load");
	GILGAMESH_CHECK_MESSAGE(
		!graph.IsCompiled() && graph.GetCompiledPassOrder().empty(),
		"failed compilation must not publish a partial execution plan");
}

// A transient shader read requires some pass to produce its contents.
GILGAMESH_TEST(
	"Engine.Render.RenderGraph",
	"CompileRejectsTransientReadWithoutWriter")
{
	RenderGraph graph;
	const auto texture = graph.CreateTexture(
		"NeverWritten",
		MakeTextureDescription());
	graph.AddPass(
		"ReadUninitialized",
		[&](GraphPassBuilder& builder)
		{
			builder.ReadPixelTexture(texture, 0);
		},
		[](ID3D11DeviceContext&) {});

	GILGAMESH_CHECK_MESSAGE(
		!graph.Compile() && !graph.IsCompiled()
			&& graph.GetCompiledPassOrder().empty(),
		"a transient texture cannot be read without any writer");
}

// Reading one produced texture in both shader stages is one logical read.
GILGAMESH_TEST(
	"Engine.Render.RenderGraph",
	"CompileAllowsVertexAndPixelReadsOfSameTexture")
{
	RenderGraph graph;
	const auto texture = graph.CreateTexture(
		"SharedInput",
		MakeTextureDescription());
	graph.AddPass(
		"ProduceInput",
		[&](GraphPassBuilder& builder)
		{
			builder.SetColorTarget(0, texture, RenderGraphLoadOp::Clear);
		},
		[](ID3D11DeviceContext&) {});
	graph.AddPass(
		"ReadInTwoStages",
		[&](GraphPassBuilder& builder)
		{
			builder.ReadVertexTexture(texture, 0);
			builder.ReadPixelTexture(texture, 0);
		},
		[](ID3D11DeviceContext&) {});

	const bool compiled = graph.Compile();
	const auto order = graph.GetCompiledPassOrder();
	GILGAMESH_CHECK_MESSAGE(
		compiled && graph.IsCompiled(),
		"vertex and pixel shaders may read the same texture in one pass");
	GILGAMESH_CHECK_MESSAGE(
		order.size() == 2 && order[0] == 0 && order[1] == 1,
		"multi-stage shader reads must not create a self-dependency");
}

// Independent Clear writers have a defined last writer by registration order.
GILGAMESH_TEST(
	"Engine.Render.RenderGraph",
	"CompileOrdersTwoClearWriters")
{
	RenderGraph graph;
	const auto texture = graph.CreateTexture(
		"SceneColor",
		MakeTextureDescription());

	graph.AddPass(
		"FirstClear",
		[&](GraphPassBuilder& builder)
		{
			builder.SetColorTarget(0, texture, RenderGraphLoadOp::Clear);
		},
		[](ID3D11DeviceContext&) {});
	graph.AddPass(
		"SecondClear",
		[&](GraphPassBuilder& builder)
		{
			builder.SetColorTarget(0, texture, RenderGraphLoadOp::Clear);
		},
		[](ID3D11DeviceContext&) {});

	const bool compiled = graph.Compile();
	const auto order = graph.GetCompiledPassOrder();
	GILGAMESH_CHECK_MESSAGE(
		compiled && graph.IsCompiled(),
		"two Clear writers may target the same texture");
	GILGAMESH_CHECK_MESSAGE(
		order.size() == 2 && order[0] == 0 && order[1] == 1,
		"the second Clear must be the final writer");
}

// Cross-resource dependencies must be rejected when they form a cycle.
GILGAMESH_TEST(
	"Engine.Render.RenderGraph",
	"CompileRejectsCrossResourceCycle")
{
	RenderGraph graph;
	const auto first = graph.CreateTexture(
		"First",
		MakeTextureDescription());
	const auto second = graph.CreateTexture(
		"Second",
		MakeTextureDescription());

	graph.AddPass(
		"ProduceFirst",
		[&](GraphPassBuilder& builder)
		{
			builder.SetColorTarget(0, first, RenderGraphLoadOp::Clear);
			builder.ReadPixelTexture(second, 0);
		},
		[](ID3D11DeviceContext&) {});
	graph.AddPass(
		"ProduceSecond",
		[&](GraphPassBuilder& builder)
		{
			builder.SetColorTarget(0, second, RenderGraphLoadOp::Clear);
			builder.ReadPixelTexture(first, 0);
		},
		[](ID3D11DeviceContext&) {});

	GILGAMESH_CHECK_MESSAGE(
		!graph.Compile(),
		"a dependency cycle across textures must fail compilation");
	GILGAMESH_CHECK_MESSAGE(
		!graph.IsCompiled() && graph.GetCompiledPassOrder().empty(),
		"a cyclic graph must not publish a partial execution plan");
}

// D3D11 cannot bind one texture for shader reading and render-target writing.
GILGAMESH_TEST(
	"Engine.Render.RenderGraph",
	"CompileRejectsSamePassReadWriteBinding")
{
	RenderGraph graph;
	const auto texture = graph.CreateTexture(
		"ConflictingTexture",
		MakeTextureDescription());
	graph.AddPass(
		"ConflictingPass",
		[&](GraphPassBuilder& builder)
		{
			builder.SetColorTarget(0, texture, RenderGraphLoadOp::Clear);
			builder.ReadPixelTexture(texture, 0);
		},
		[](ID3D11DeviceContext&) {});

	GILGAMESH_CHECK_MESSAGE(
		!graph.Compile() && !graph.IsCompiled(),
		"one pass must not use the same texture as SRV and color target");
}

// Attachments in one render-target set must have compatible dimensions.
GILGAMESH_TEST(
	"Engine.Render.RenderGraph",
	"CompileRejectsMismatchedAttachmentExtents")
{
	RenderGraph graph;
	const auto first = graph.CreateTexture(
		"FirstColor",
		MakeTextureDescription());
	auto secondDescription = MakeTextureDescription();
	secondDescription.extent.width = 64;
	const auto second = graph.CreateTexture(
		"SecondColor",
		secondDescription);
	graph.AddPass(
		"MismatchedAttachments",
		[&](GraphPassBuilder& builder)
		{
			builder.SetColorTarget(0, first, RenderGraphLoadOp::Clear);
			builder.SetColorTarget(1, second, RenderGraphLoadOp::Clear);
		},
		[](ID3D11DeviceContext&) {});

	GILGAMESH_CHECK_MESSAGE(
		!graph.Compile() && !graph.IsCompiled(),
		"one pass cannot combine color targets with different extents");
}

// Each shader stage and attachment set owns its slots independently.
GILGAMESH_TEST(
	"Engine.Render.RenderGraph",
	"CompileRejectsDuplicateBindingSlots")
{
	RenderGraph graph;
	const auto first = graph.CreateTexture(
		"FirstColor",
		MakeTextureDescription());
	const auto second = graph.CreateTexture(
		"SecondColor",
		MakeTextureDescription());
	graph.AddPass(
		"ProduceColors",
		[&](GraphPassBuilder& builder)
		{
			builder.SetColorTarget(0, first, RenderGraphLoadOp::Clear);
			builder.SetColorTarget(1, second, RenderGraphLoadOp::Clear);
		},
		[](ID3D11DeviceContext&) {});
	graph.AddPass(
		"DuplicatePixelSlot",
		[&](GraphPassBuilder& builder)
		{
			builder.ReadPixelTexture(first, 0);
			builder.ReadPixelTexture(second, 0);
		},
		[](ID3D11DeviceContext&) {});

	GILGAMESH_CHECK_MESSAGE(
		!graph.Compile() && !graph.IsCompiled(),
		"two pixel shader resources cannot occupy one binding slot");

	// A separate graph isolates the color-target slot conflict.
	RenderGraph targetGraph;
	const auto targetFirst = targetGraph.CreateTexture(
		"FirstTarget",
		MakeTextureDescription());
	const auto targetSecond = targetGraph.CreateTexture(
		"SecondTarget",
		MakeTextureDescription());
	targetGraph.AddPass(
		"DuplicateTargetSlot",
		[&](GraphPassBuilder& builder)
		{
			builder.SetColorTarget(0, targetFirst, RenderGraphLoadOp::Clear);
			builder.SetColorTarget(0, targetSecond, RenderGraphLoadOp::Clear);
		},
		[](ID3D11DeviceContext&) {});

	GILGAMESH_CHECK_MESSAGE(
		!targetGraph.Compile() && !targetGraph.IsCompiled(),
		"two render targets cannot occupy one color-target slot");
}

// A read-only DSV cannot accept a depth clear operation.
GILGAMESH_TEST(
	"Engine.Render.RenderGraph",
	"CompileRejectsClearOnReadOnlyDepth")
{
	RenderGraph graph;
	const auto depth = graph.CreateTexture(
		"ReadOnlyDepth",
		MakeTextureDescription(DXGI_FORMAT_D32_FLOAT));
	graph.AddPass(
		"InvalidDepthClear",
		[&](GraphPassBuilder& builder)
		{
			builder.SetDepthTarget(
				depth,
				RenderGraphDepthAccess::ReadOnly,
				RenderGraphLoadOp::Clear);
		},
		[](ID3D11DeviceContext&) {});

	GILGAMESH_CHECK_MESSAGE(
		!graph.Compile() && !graph.IsCompiled(),
		"read-only depth access must not clear its texture");
}

// One logical texture may require several native view types across passes.
GILGAMESH_TEST(
	"Engine.Render.RenderGraph",
	"CompileAcceptsTexturesRequiringMultipleViewTypes")
{
	RenderGraph graph;
	const auto color = graph.CreateTexture(
		"SceneColor",
		MakeTextureDescription());
	const auto depth = graph.CreateTexture(
		"SceneDepth",
		MakeTextureDescription(DXGI_FORMAT_D32_FLOAT));

	graph.AddPass(
		"InitializeAttachments",
		[&](GraphPassBuilder& builder)
		{
			builder.SetColorTarget(
				0,
				color,
				RenderGraphLoadOp::Clear);
			builder.SetDepthTarget(
				depth,
				RenderGraphDepthAccess::ReadWrite,
				RenderGraphLoadOp::Clear);
		},
		[](ID3D11DeviceContext&) {});

	graph.AddPass(
		"ReadOnlyDepth",
		[&](GraphPassBuilder& builder)
		{
			builder.SetDepthTarget(
				depth,
				RenderGraphDepthAccess::ReadOnly,
				RenderGraphLoadOp::Load);
		},
		[](ID3D11DeviceContext&) {});

	graph.AddPass(
		"SampleAttachments",
		[&](GraphPassBuilder& builder)
		{
			builder.ReadVertexTexture(color, 0);
			builder.ReadPixelTexture(color, 0);
			builder.ReadPixelTexture(depth, 1);
		},
		[](ID3D11DeviceContext&) {});

	const bool compiled = graph.Compile();
	const auto order = graph.GetCompiledPassOrder();
	GILGAMESH_CHECK_MESSAGE(
		compiled && graph.IsCompiled(),
		"compatible RTV, writable/read-only DSV, and SRV requirements must compile");
	GILGAMESH_CHECK_MESSAGE(
		order.size() == 3 && order[0] == 0
			&& order[1] == 1 && order[2] == 2,
		"view requirements must not change the dependency order");
}

// Invalid enum values must not silently turn into another access mode.
GILGAMESH_TEST(
	"Engine.Render.RenderGraph",
	"CompileRejectsUnknownLoadAndDepthAccessValues")
{
	const auto invalidLoad = static_cast<RenderGraphLoadOp>(255);
	const auto invalidDepthAccess = static_cast<RenderGraphDepthAccess>(255);

	// Color attachments reject an unknown load operation.
	RenderGraph colorGraph;
	const auto color = colorGraph.CreateTexture(
		"Color",
		MakeTextureDescription());
	colorGraph.AddPass(
		"InvalidColorLoad",
		[&](GraphPassBuilder& builder)
		{
			builder.SetColorTarget(0, color, invalidLoad);
		},
		[](ID3D11DeviceContext&) {});
	GILGAMESH_CHECK_MESSAGE(
		!colorGraph.Compile() && !colorGraph.IsCompiled(),
		"color attachments must reject unknown load operations");

	// Depth attachments validate both independent enum arguments.
	RenderGraph depthLoadGraph;
	const auto firstDepth = depthLoadGraph.CreateTexture(
		"DepthLoad",
		MakeTextureDescription(DXGI_FORMAT_D32_FLOAT));
	depthLoadGraph.AddPass(
		"InvalidDepthLoad",
		[&](GraphPassBuilder& builder)
		{
			builder.SetDepthTarget(
				firstDepth,
				RenderGraphDepthAccess::ReadWrite,
				invalidLoad);
		},
		[](ID3D11DeviceContext&) {});
	GILGAMESH_CHECK_MESSAGE(
		!depthLoadGraph.Compile() && !depthLoadGraph.IsCompiled(),
		"depth attachments must reject unknown load operations");

	RenderGraph depthAccessGraph;
	const auto secondDepth = depthAccessGraph.CreateTexture(
		"DepthAccess",
		MakeTextureDescription(DXGI_FORMAT_D32_FLOAT));
	depthAccessGraph.AddPass(
		"InvalidDepthAccess",
		[&](GraphPassBuilder& builder)
		{
			builder.SetDepthTarget(
				secondDepth,
				invalidDepthAccess,
				RenderGraphLoadOp::Clear);
		},
		[](ID3D11DeviceContext&) {});
	GILGAMESH_CHECK_MESSAGE(
		!depthAccessGraph.Compile() && !depthAccessGraph.IsCompiled(),
		"depth attachments must reject unknown access modes");
}

// Recompilation is required whenever a graph declaration changes.
GILGAMESH_TEST(
	"Engine.Render.RenderGraph",
	"CompileInvalidatesPlanOnMutationAndFailure")
{
	RenderGraph graph;
	const auto texture = graph.CreateTexture(
		"SceneColor",
		MakeTextureDescription());

	graph.AddPass(
		"Initialize",
		[&](GraphPassBuilder& builder)
		{
			builder.SetColorTarget(0, texture, RenderGraphLoadOp::Clear);
		},
		[](ID3D11DeviceContext&) {});

	GILGAMESH_CHECK_MESSAGE(
		graph.Compile() && graph.IsCompiled()
			&& graph.GetCompiledPassOrder().size() == 1,
		"a valid graph must publish its compiled order");
	(void)graph.CreateTexture("UnusedNewTexture", MakeTextureDescription());
	GILGAMESH_CHECK_MESSAGE(
		!graph.IsCompiled() && graph.GetCompiledPassOrder().empty(),
		"adding a texture must invalidate the previous compiled plan");
	GILGAMESH_CHECK_MESSAGE(
		graph.Compile() && graph.IsCompiled(),
		"a graph with an unused texture must still recompile");

	// A new declaration discards the old plan before the next compile attempt.
	graph.AddPass(
		"InvalidReader",
		[](GraphPassBuilder& builder)
		{
			builder.ReadPixelTexture(RenderGraphTextureHandle{}, 0);
		},
		[](ID3D11DeviceContext&) {});
	GILGAMESH_CHECK_MESSAGE(
		!graph.IsCompiled() && graph.GetCompiledPassOrder().empty(),
		"adding a pass must invalidate the previous compiled plan");
	GILGAMESH_CHECK_MESSAGE(
		!graph.Compile() && !graph.IsCompiled()
			&& graph.GetCompiledPassOrder().empty(),
		"a failed recompile must leave no stale or partial plan");

	graph.Reset();
	GILGAMESH_CHECK_MESSAGE(
		!graph.IsCompiled() && graph.GetCompiledPassOrder().empty(),
		"Reset must leave the graph uncompiled");
}
