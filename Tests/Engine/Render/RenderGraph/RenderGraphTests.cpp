#include "Engine/Render/RenderGraph/RenderGraph.h"
#include "Tests/TestFramework.h"

#include <array>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <vector>
#include <wrl/client.h>

namespace
{
	Microsoft::WRL::ComPtr<ID3D11Device> CreateRenderGraphTestDevice()
	{
		Microsoft::WRL::ComPtr<ID3D11Device> device;
		Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
		const HRESULT result = D3D11CreateDevice(
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
		if (FAILED(result))
		{
			device.Reset();
		}
		return device;
	}

	ID3D11Device* GetRenderGraphTestDevice()
	{
		static Microsoft::WRL::ComPtr<ID3D11Device> device =
			CreateRenderGraphTestDevice();
		return device.Get();
	}

	Microsoft::WRL::ComPtr<ID3D11DeviceContext>
		GetRenderGraphTestContext()
	{
		Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
		if (ID3D11Device* device = GetRenderGraphTestDevice())
		{
			device->GetImmediateContext(context.GetAddressOf());
		}
		return context;
	}

	// Copies one non-multisampled RGBA8 texture to staging memory for verification.
	bool ReadFirstRgba8Pixel(
		ID3D11Device* device,
		ID3D11DeviceContext* context,
		ID3D11Texture2D* texture,
		std::array<std::uint8_t, 4>& pixel)
	{
		if (!device || !context || !texture)
		{
			return false;
		}

		D3D11_TEXTURE2D_DESC description{};
		texture->GetDesc(&description);
		description.Usage = D3D11_USAGE_STAGING;
		description.BindFlags = 0;
		description.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
		description.MiscFlags = 0;

		Microsoft::WRL::ComPtr<ID3D11Texture2D> stagingTexture;
		if (FAILED(device->CreateTexture2D(
			&description,
			nullptr,
			stagingTexture.GetAddressOf())))
		{
			return false;
		}

		context->CopyResource(stagingTexture.Get(), texture);
		D3D11_MAPPED_SUBRESOURCE mapped{};
		if (FAILED(context->Map(
			stagingTexture.Get(),
			0,
			D3D11_MAP_READ,
			0,
			&mapped)))
		{
			return false;
		}

		const auto* source = static_cast<const std::uint8_t*>(mapped.pData);
		for (std::size_t channel = 0; channel < pixel.size(); ++channel)
		{
			pixel[channel] = source[channel];
		}
		context->Unmap(stagingTexture.Get(), 0);
		return true;
	}

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

	const bool compiled = graph.Compile(GetRenderGraphTestDevice());
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

	const bool compiled = graph.Compile(GetRenderGraphTestDevice());
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

	const bool compiled = graph.Compile(GetRenderGraphTestDevice());
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

	const bool compiled = graph.Compile(device.Get());
	const auto order = graph.GetCompiledPassOrder();
	GILGAMESH_CHECK_MESSAGE(
		compiled && graph.IsCompiled(),
		"an imported texture may expose its initial contents to a reader");
	GILGAMESH_CHECK_MESSAGE(
		order.size() == 2 && order[0] == 0 && order[1] == 1,
		"the initial read must precede the graph's first imported writer");
}

// Imported native metadata must agree with the graph declaration.
GILGAMESH_TEST(
	"Engine.Render.RenderGraph",
	"CompileRejectsMismatchedImportedDescription")
{
	ID3D11Device* device = GetRenderGraphTestDevice();
	GILGAMESH_CHECK_MESSAGE(
		device != nullptr,
		"the WARP device must be available for imported texture validation");
	if (!device) return;

	D3D11_TEXTURE2D_DESC nativeDescription{};
	nativeDescription.Width = 64;
	nativeDescription.Height = 64;
	nativeDescription.MipLevels = 1;
	nativeDescription.ArraySize = 1;
	nativeDescription.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	nativeDescription.SampleDesc.Count = 1;
	nativeDescription.Usage = D3D11_USAGE_DEFAULT;
	nativeDescription.BindFlags = D3D11_BIND_RENDER_TARGET;

	Microsoft::WRL::ComPtr<ID3D11Texture2D> nativeTexture;
	Microsoft::WRL::ComPtr<ID3D11RenderTargetView> renderTargetView;
	HRESULT result = device->CreateTexture2D(
		&nativeDescription,
		nullptr,
		nativeTexture.GetAddressOf());
	if (SUCCEEDED(result))
	{
		result = device->CreateRenderTargetView(
			nativeTexture.Get(),
			nullptr,
			renderTargetView.GetAddressOf());
	}
	GILGAMESH_CHECK_MESSAGE(
		SUCCEEDED(result) && nativeTexture && renderTargetView,
		"the mismatched imported texture fixture must be created");
	if (FAILED(result) || !nativeTexture || !renderTargetView) return;

	ImportedRenderGraphTexture imported;
	imported.texture = nativeTexture.Get();
	imported.renderTargetView = renderTargetView.Get();

	RenderGraph graph;
	const auto texture = graph.ImportTexture(
		"MismatchedImport",
		MakeTextureDescription(),
		imported);
	graph.AddPass(
		"WriteImport",
		[&](GraphPassBuilder& builder)
		{
			builder.SetColorTarget(
				0,
				texture,
				RenderGraphLoadOp::Clear);
		},
		[](ID3D11DeviceContext&) {});

	GILGAMESH_CHECK_MESSAGE(
		!graph.Compile(device) && !graph.IsCompiled()
			&& graph.GetCompiledPassOrder().empty(),
		"an imported texture with mismatched native metadata must be rejected");
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
		!graph.Compile(device.Get()) && !graph.IsCompiled()
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
		!graph.Compile(GetRenderGraphTestDevice()),
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
		!graph.Compile(GetRenderGraphTestDevice()) && !graph.IsCompiled()
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

	const bool compiled = graph.Compile(GetRenderGraphTestDevice());
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

	const bool compiled = graph.Compile(GetRenderGraphTestDevice());
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
		!graph.Compile(GetRenderGraphTestDevice()),
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
		!graph.Compile(GetRenderGraphTestDevice()) && !graph.IsCompiled(),
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
		!graph.Compile(GetRenderGraphTestDevice()) && !graph.IsCompiled(),
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
		!graph.Compile(GetRenderGraphTestDevice()) && !graph.IsCompiled(),
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
		!targetGraph.Compile(GetRenderGraphTestDevice()) && !targetGraph.IsCompiled(),
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
		!graph.Compile(GetRenderGraphTestDevice()) && !graph.IsCompiled(),
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

	const bool compiled = graph.Compile(GetRenderGraphTestDevice());
	const auto order = graph.GetCompiledPassOrder();
	GILGAMESH_CHECK_MESSAGE(
		compiled && graph.IsCompiled(),
		"compatible RTV, writable/read-only DSV, and SRV requirements must compile");
	GILGAMESH_CHECK_MESSAGE(
		order.size() == 3 && order[0] == 0
			&& order[1] == 1 && order[2] == 2,
		"view requirements must not change the dependency order");
}

// Resource creation failure must not publish a partial compiled graph.
GILGAMESH_TEST(
	"Engine.Render.RenderGraph",
	"CompileRequiresDeviceForGraphOwnedResources")
{
	RenderGraph graph;
	const auto color = graph.CreateTexture(
		"SceneColor",
		MakeTextureDescription());

	graph.AddPass(
		"InitializeColor",
		[&](GraphPassBuilder& builder)
		{
			builder.SetColorTarget(
				0,
				color,
				RenderGraphLoadOp::Clear);
		},
		[](ID3D11DeviceContext&) {});

	GILGAMESH_CHECK_MESSAGE(
		!graph.Compile(nullptr) && !graph.IsCompiled()
			&& graph.GetCompiledPassOrder().empty(),
		"resource creation without a device must not publish a partial plan");
	GILGAMESH_CHECK_MESSAGE(
		graph.Compile(GetRenderGraphTestDevice()) && graph.IsCompiled(),
		"the same declarations must remain compilable with a valid device");
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
		!colorGraph.Compile(GetRenderGraphTestDevice()) && !colorGraph.IsCompiled(),
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
		!depthLoadGraph.Compile(GetRenderGraphTestDevice()) && !depthLoadGraph.IsCompiled(),
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
		!depthAccessGraph.Compile(GetRenderGraphTestDevice()) && !depthAccessGraph.IsCompiled(),
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
		graph.Compile(GetRenderGraphTestDevice()) && graph.IsCompiled()
			&& graph.GetCompiledPassOrder().size() == 1,
		"a valid graph must publish its compiled order");
	(void)graph.CreateTexture("UnusedNewTexture", MakeTextureDescription());
	GILGAMESH_CHECK_MESSAGE(
		!graph.IsCompiled() && graph.GetCompiledPassOrder().empty(),
		"adding a texture must invalidate the previous compiled plan");
	GILGAMESH_CHECK_MESSAGE(
		graph.Compile(GetRenderGraphTestDevice()) && graph.IsCompiled(),
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
		!graph.Compile(GetRenderGraphTestDevice()) && !graph.IsCompiled()
			&& graph.GetCompiledPassOrder().empty(),
		"a failed recompile must leave no stale or partial plan");

	graph.Reset();
	GILGAMESH_CHECK_MESSAGE(
		!graph.IsCompiled() && graph.GetCompiledPassOrder().empty(),
		"Reset must leave the graph uncompiled");
}

// Execution is valid only for a compiled graph and a live device context.
GILGAMESH_TEST(
	"Engine.Render.RenderGraph",
	"ExecuteRequiresCompiledGraphAndContext")
{
	Microsoft::WRL::ComPtr<ID3D11DeviceContext> context =
		GetRenderGraphTestContext();
	GILGAMESH_CHECK_MESSAGE(
		context != nullptr,
		"the WARP immediate context must be available for graph execution");
	if (!context) return;

	RenderGraph graph;
	int executeCalls = 0;
	graph.AddPass(
		"EmptyPass",
		[](GraphPassBuilder&) {},
		[&](ID3D11DeviceContext&)
		{
			++executeCalls;
		});

	GILGAMESH_CHECK_MESSAGE(
		!graph.Execute(context.Get()) && executeCalls == 0,
		"an uncompiled graph must reject execution without invoking callbacks");
	GILGAMESH_CHECK_MESSAGE(
		graph.Compile(GetRenderGraphTestDevice()),
		"the empty execution graph must compile");
	GILGAMESH_CHECK_MESSAGE(
		!graph.Execute(nullptr) && executeCalls == 0,
		"execution must reject a null device context");
	GILGAMESH_CHECK_MESSAGE(
		graph.Execute(context.Get()) && executeCalls == 1,
		"a compiled graph must invoke its callback exactly once");

	context->ClearState();
}

// Execute follows the compiled DAG order rather than AddPass registration order.
GILGAMESH_TEST(
	"Engine.Render.RenderGraph",
	"ExecuteUsesCompiledPassOrder")
{
	Microsoft::WRL::ComPtr<ID3D11DeviceContext> context =
		GetRenderGraphTestContext();
	GILGAMESH_CHECK_MESSAGE(
		context != nullptr,
		"the WARP immediate context must be available for ordered execution");
	if (!context) return;
	context->ClearState();

	RenderGraph graph;
	const RenderGraphTextureHandle texture = graph.CreateTexture(
		"ProducedTexture",
		MakeTextureDescription());
	std::vector<int> executionOrder;

	graph.AddPass(
		"RegisteredReader",
		[&](GraphPassBuilder& builder)
		{
			builder.ReadPixelTexture(texture, 3);
		},
		[&](ID3D11DeviceContext&)
		{
			executionOrder.push_back(0);
		});
	graph.AddPass(
		"RegisteredProducer",
		[&](GraphPassBuilder& builder)
		{
			builder.SetColorTarget(
				0,
				texture,
				RenderGraphLoadOp::Clear);
		},
		[&](ID3D11DeviceContext&)
		{
			executionOrder.push_back(1);
		});

	GILGAMESH_CHECK_MESSAGE(
		graph.Compile(GetRenderGraphTestDevice()),
		"the producer-reader graph must compile");
	GILGAMESH_CHECK_MESSAGE(
		graph.Execute(context.Get()),
		"the producer-reader graph must execute");
	const std::vector<int> expectedOrder{ 1, 0 };
	GILGAMESH_CHECK_MESSAGE(
		executionOrder == expectedOrder,
		"execution must run the producer before its earlier-registered reader");

	context->ClearState();
}

// A compiled graph may execute only on a context from its resource device.
GILGAMESH_TEST(
	"Engine.Render.RenderGraph",
	"ExecuteRejectsContextFromAnotherDevice")
{
	Microsoft::WRL::ComPtr<ID3D11Device> otherDevice;
	Microsoft::WRL::ComPtr<ID3D11DeviceContext> otherContext;
	const HRESULT createResult = D3D11CreateDevice(
		nullptr,
		D3D_DRIVER_TYPE_WARP,
		nullptr,
		0,
		nullptr,
		0,
		D3D11_SDK_VERSION,
		otherDevice.GetAddressOf(),
		nullptr,
		otherContext.GetAddressOf());
	GILGAMESH_CHECK_MESSAGE(
		SUCCEEDED(createResult) && otherDevice && otherContext,
		"a second WARP device must be available for context validation");
	if (FAILED(createResult) || !otherDevice || !otherContext) return;

	RenderGraph graph;
	int executeCalls = 0;
	graph.AddPass(
		"DeviceBoundPass",
		[](GraphPassBuilder&) {},
		[&](ID3D11DeviceContext&)
		{
			++executeCalls;
		});

	GILGAMESH_CHECK_MESSAGE(
		graph.Compile(GetRenderGraphTestDevice()),
		"the device-bound execution graph must compile");
	GILGAMESH_CHECK_MESSAGE(
		!graph.Execute(otherContext.Get()) && executeCalls == 0,
		"a context from another device must be rejected before callbacks run");
}

// Callback failures must not leave graph bindings or execution state behind.
GILGAMESH_TEST(
	"Engine.Render.RenderGraph",
	"ExecuteCleansUpAfterCallbackFailure")
{
	Microsoft::WRL::ComPtr<ID3D11DeviceContext> context =
		GetRenderGraphTestContext();
	GILGAMESH_CHECK_MESSAGE(
		context != nullptr,
		"the WARP immediate context must be available for failure cleanup");
	if (!context) return;
	context->ClearState();

	RenderGraph graph;
	const RenderGraphTextureHandle color = graph.CreateTexture(
		"FailureTarget",
		MakeTextureDescription());
	bool shouldThrow = true;
	int executeCalls = 0;
	graph.AddPass(
		"FailOnce",
		[&](GraphPassBuilder& builder)
		{
			builder.SetColorTarget(
				0,
				color,
				RenderGraphLoadOp::Clear);
		},
		[&](ID3D11DeviceContext&)
		{
			++executeCalls;
			if (shouldThrow)
			{
				throw std::runtime_error("expected render failure");
			}
		});

	GILGAMESH_CHECK_MESSAGE(
		graph.Compile(GetRenderGraphTestDevice()),
		"the callback-failure graph must compile");
	bool callbackThrew = false;
	try
	{
		(void)graph.Execute(context.Get());
	}
	catch (const std::runtime_error&)
	{
		callbackThrew = true;
	}

	Microsoft::WRL::ComPtr<ID3D11RenderTargetView> target;
	Microsoft::WRL::ComPtr<ID3D11DepthStencilView> depthTarget;
	context->OMGetRenderTargets(
		1,
		target.GetAddressOf(),
		depthTarget.GetAddressOf());
	GILGAMESH_CHECK_MESSAGE(
		callbackThrew && !target && !depthTarget,
		"a callback exception must propagate after output bindings are cleared");

	shouldThrow = false;
	GILGAMESH_CHECK_MESSAGE(
		graph.Execute(context.Get()) && executeCalls == 2,
		"a graph must be executable again after callback failure cleanup");

	context->ClearState();
}

// Each callback sees exactly the native views declared for that pass.
GILGAMESH_TEST(
	"Engine.Render.RenderGraph",
	"ExecuteBindsAndUnbindsDeclaredTextureViews")
{
	Microsoft::WRL::ComPtr<ID3D11DeviceContext> context =
		GetRenderGraphTestContext();
	GILGAMESH_CHECK_MESSAGE(
		context != nullptr,
		"the WARP immediate context must be available for binding verification");
	if (!context) return;
	context->ClearState();

	RenderGraph graph;
	const RenderGraphTextureHandle color = graph.CreateTexture(
		"SceneColor",
		MakeTextureDescription());
	const RenderGraphTextureHandle depth = graph.CreateTexture(
		"SceneDepth",
		MakeTextureDescription(DXGI_FORMAT_D32_FLOAT));
	Microsoft::WRL::ComPtr<ID3D11Resource> writtenColorResource;
	Microsoft::WRL::ComPtr<ID3D11Resource> writtenDepthResource;

	graph.AddPass(
		"WriteAttachments",
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
		[&](ID3D11DeviceContext& passContext)
		{
			Microsoft::WRL::ComPtr<ID3D11RenderTargetView> target;
			Microsoft::WRL::ComPtr<ID3D11DepthStencilView> depthTarget;
			D3D11_VIEWPORT viewport{};
			UINT viewportCount = 1;
			passContext.OMGetRenderTargets(
				1,
				target.GetAddressOf(),
				depthTarget.GetAddressOf());
			passContext.RSGetViewports(&viewportCount, &viewport);

			GILGAMESH_CHECK_MESSAGE(
				target != nullptr && depthTarget != nullptr,
				"the writer callback must see its declared RTV and DSV");
			GILGAMESH_CHECK_MESSAGE(
				viewportCount == 1
					&& viewport.Width == 128.0f
					&& viewport.Height == 128.0f
					&& viewport.MinDepth == 0.0f
					&& viewport.MaxDepth == 1.0f,
				"an attachment pass must receive a full-size default viewport");
			if (target)
			{
				target->GetResource(writtenColorResource.GetAddressOf());
			}
			if (depthTarget)
			{
				D3D11_DEPTH_STENCIL_VIEW_DESC viewDescription{};
				depthTarget->GetDesc(&viewDescription);
				GILGAMESH_CHECK_MESSAGE(
					viewDescription.Flags == 0,
					"read-write depth access must bind the writable DSV");
				depthTarget->GetResource(writtenDepthResource.GetAddressOf());
			}
		});

	graph.AddPass(
		"ReadColorWithReadOnlyDepth",
		[&](GraphPassBuilder& builder)
		{
			builder.SetDepthTarget(
				depth,
				RenderGraphDepthAccess::ReadOnly,
				RenderGraphLoadOp::Load);
			builder.ReadVertexTexture(color, 4);
			builder.ReadPixelTexture(color, 7);
		},
		[&](ID3D11DeviceContext& passContext)
		{
			Microsoft::WRL::ComPtr<ID3D11RenderTargetView> target;
			Microsoft::WRL::ComPtr<ID3D11DepthStencilView> depthTarget;
			Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> vertexView;
			Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> pixelView;
			passContext.OMGetRenderTargets(
				1,
				target.GetAddressOf(),
				depthTarget.GetAddressOf());
			passContext.VSGetShaderResources(
				4,
				1,
				vertexView.GetAddressOf());
			passContext.PSGetShaderResources(
				7,
				1,
				pixelView.GetAddressOf());

			GILGAMESH_CHECK_MESSAGE(
				target == nullptr && depthTarget != nullptr,
				"the read pass must bind only its declared output attachment");
			GILGAMESH_CHECK_MESSAGE(
				vertexView != nullptr && pixelView != nullptr,
				"the read pass must bind both declared shader-stage slots");

			if (depthTarget)
			{
				D3D11_DEPTH_STENCIL_VIEW_DESC viewDescription{};
				depthTarget->GetDesc(&viewDescription);
				Microsoft::WRL::ComPtr<ID3D11Resource> resource;
				depthTarget->GetResource(resource.GetAddressOf());
				GILGAMESH_CHECK_MESSAGE(
					(viewDescription.Flags & D3D11_DSV_READ_ONLY_DEPTH) != 0
						&& resource.Get() == writtenDepthResource.Get(),
					"read-only depth access must select the read-only view of the written texture");
			}

			Microsoft::WRL::ComPtr<ID3D11Resource> vertexResource;
			Microsoft::WRL::ComPtr<ID3D11Resource> pixelResource;
			if (vertexView)
			{
				vertexView->GetResource(vertexResource.GetAddressOf());
			}
			if (pixelView)
			{
				pixelView->GetResource(pixelResource.GetAddressOf());
			}
			GILGAMESH_CHECK_MESSAGE(
				vertexResource.Get() == writtenColorResource.Get()
					&& pixelResource.Get() == writtenColorResource.Get(),
				"shader reads must refer to the color texture produced by the writer");
		});

	graph.AddPass(
		"ObservePreviousPassCleanup",
		[](GraphPassBuilder&) {},
		[&](ID3D11DeviceContext& passContext)
		{
			Microsoft::WRL::ComPtr<ID3D11RenderTargetView> target;
			Microsoft::WRL::ComPtr<ID3D11DepthStencilView> depthTarget;
			Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> vertexView;
			Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> pixelView;
			passContext.OMGetRenderTargets(
				1,
				target.GetAddressOf(),
				depthTarget.GetAddressOf());
			passContext.VSGetShaderResources(
				4,
				1,
				vertexView.GetAddressOf());
			passContext.PSGetShaderResources(
				7,
				1,
				pixelView.GetAddressOf());

			GILGAMESH_CHECK_MESSAGE(
				!target && !depthTarget && !vertexView && !pixelView,
				"a completed pass must leave its OM and shader-resource bindings clear");
		});

	GILGAMESH_CHECK_MESSAGE(
		graph.Compile(GetRenderGraphTestDevice()),
		"the attachment and shader-read graph must compile");
	GILGAMESH_CHECK_MESSAGE(
		graph.Execute(context.Get()),
		"the attachment and shader-read graph must execute");

	Microsoft::WRL::ComPtr<ID3D11RenderTargetView> finalTarget;
	Microsoft::WRL::ComPtr<ID3D11DepthStencilView> finalDepthTarget;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> finalVertexView;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> finalPixelView;
	context->OMGetRenderTargets(
		1,
		finalTarget.GetAddressOf(),
		finalDepthTarget.GetAddressOf());
	context->VSGetShaderResources(
		4,
		1,
		finalVertexView.GetAddressOf());
	context->PSGetShaderResources(
		7,
		1,
		finalPixelView.GetAddressOf());
	GILGAMESH_CHECK_MESSAGE(
		!finalTarget && !finalDepthTarget && !finalVertexView && !finalPixelView,
		"Execute must return with graph-managed OM and shader-resource slots unbound");

	context->ClearState();
}

// Load preserves imported contents while Clear replaces them before the callback.
GILGAMESH_TEST(
	"Engine.Render.RenderGraph",
	"ExecuteHonorsColorLoadOperations")
{
	ID3D11Device* device = GetRenderGraphTestDevice();
	Microsoft::WRL::ComPtr<ID3D11DeviceContext> context =
		GetRenderGraphTestContext();
	GILGAMESH_CHECK_MESSAGE(
		device != nullptr && context != nullptr,
		"the WARP device and context must be available for load-operation verification");
	if (!device || !context) return;
	context->ClearState();

	D3D11_TEXTURE2D_DESC nativeDescription{};
	nativeDescription.Width = 128;
	nativeDescription.Height = 128;
	nativeDescription.MipLevels = 1;
	nativeDescription.ArraySize = 1;
	nativeDescription.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	nativeDescription.SampleDesc.Count = 1;
	nativeDescription.Usage = D3D11_USAGE_DEFAULT;
	nativeDescription.BindFlags = D3D11_BIND_RENDER_TARGET;

	Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
	Microsoft::WRL::ComPtr<ID3D11RenderTargetView> target;
	HRESULT result = device->CreateTexture2D(
		&nativeDescription,
		nullptr,
		texture.GetAddressOf());
	if (SUCCEEDED(result))
	{
		result = device->CreateRenderTargetView(
			texture.Get(),
			nullptr,
			target.GetAddressOf());
	}
	GILGAMESH_CHECK_MESSAGE(
		SUCCEEDED(result) && texture && target,
		"the imported color-target fixture must be created");
	if (FAILED(result) || !texture || !target) return;

	ImportedRenderGraphTexture imported;
	imported.texture = texture.Get();
	imported.renderTargetView = target.Get();
	const Color4 initialColor{ 1.0f, 0.0f, 1.0f, 1.0f };
	context->ClearRenderTargetView(target.Get(), initialColor.Data());

	RenderGraph loadGraph;
	const RenderGraphTextureHandle loadTarget = loadGraph.ImportTexture(
		"LoadedColor",
		MakeTextureDescription(),
		imported);
	loadGraph.AddPass(
		"PreserveColor",
		[&](GraphPassBuilder& builder)
		{
			builder.SetColorTarget(
				0,
				loadTarget,
				RenderGraphLoadOp::Load);
		},
		[](ID3D11DeviceContext&) {});
	GILGAMESH_CHECK_MESSAGE(
		loadGraph.Compile(device) && loadGraph.Execute(context.Get()),
		"the imported Load pass must compile and execute");

	context->OMSetRenderTargets(0, nullptr, nullptr);
	std::array<std::uint8_t, 4> loadedPixel{};
	const bool readLoadedPixel = ReadFirstRgba8Pixel(
		device,
		context.Get(),
		texture.Get(),
		loadedPixel);
	const std::array<std::uint8_t, 4> expectedLoadedPixel{
		255, 0, 255, 255
	};
	GILGAMESH_CHECK_MESSAGE(
		readLoadedPixel && loadedPixel == expectedLoadedPixel,
		"Load must preserve the imported render target's existing contents");

	RenderGraph clearGraph;
	const RenderGraphTextureHandle clearTarget = clearGraph.ImportTexture(
		"ClearedColor",
		MakeTextureDescription(),
		imported);
	clearGraph.AddPass(
		"ClearColor",
		[&](GraphPassBuilder& builder)
		{
			builder.SetColorTarget(
				0,
				clearTarget,
				RenderGraphLoadOp::Clear,
				Color4{ 0.0f, 1.0f, 0.0f, 1.0f });
		},
		[](ID3D11DeviceContext&) {});
	GILGAMESH_CHECK_MESSAGE(
		clearGraph.Compile(device) && clearGraph.Execute(context.Get()),
		"the imported Clear pass must compile and execute");

	context->OMSetRenderTargets(0, nullptr, nullptr);
	std::array<std::uint8_t, 4> clearedPixel{};
	const bool readClearedPixel = ReadFirstRgba8Pixel(
		device,
		context.Get(),
		texture.Get(),
		clearedPixel);
	const std::array<std::uint8_t, 4> expectedClearedPixel{
		0, 255, 0, 255
	};
	GILGAMESH_CHECK_MESSAGE(
		readClearedPixel && clearedPixel == expectedClearedPixel,
		"Clear must replace the render target contents before the callback");

	context->ClearState();
}
