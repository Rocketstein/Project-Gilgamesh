#include "EditorProgram.h"

#include "Camera/EditorCamera.h"
#include "Engine/Core/Logging/Logger.h"
#include "Engine/Render/Buffers/ConstantBuffers.h"
#include "Engine/Render/Pipeline/GraphicsPipeline.h"
#include "Engine/Render/Renderer/Renderer.h"
#include "Engine/Render/VertexTypes/VertexTypes.h"
#include "Engine/Runtime/EngineServices.h"
#include "Engine/Runtime/FrameContext.h"
#include "Engine/Runtime/RenderContext.h"

#if GILGAMESH_ENABLE_DEVELOPER_TOOLS
    #include "DeveloperTools/Console/Commands/BuiltInCommands.h"
    #include "DeveloperTools/Console/ConsoleBuffer.h"
    #include "DeveloperTools/Console/ConsoleCommandOutput.h"
    #include "DeveloperTools/Console/ConsoleConfiguration.h"
    #include "DeveloperTools/Console/ConsoleLogSink.h"
    #include "DeveloperTools/Console/OutputLogPanel.h"
    #include "DeveloperTools/Runtime/ImGuiIntegration.h"
    #include "DeveloperTools/Workspace/DeveloperToolsWorkspace.h"
#endif

struct EditorProgram::Impl
{
    // Core
    Window* window = nullptr;
    Renderer* renderer = nullptr;

	// Developer Tools
#if GILGAMESH_ENABLE_DEVELOPER_TOOLS
    ImGuiIntegration imgui;
    DeveloperToolsWorkspace workspace;

    // Console
    std::shared_ptr<ConsoleBuffer> consoleBuffer;
    std::shared_ptr<ConsoleLogSink> consoleSink;
    LogSinkRegistration consoleRegistration;
    ConsoleConfiguration consoleConfiguration;
    std::unique_ptr<OutputLogPanel> outputLogPanel;

    // Console Commands
    std::unique_ptr<ConsoleCommandOutput> commandOutput;
    CommandRegistry commandRegistry;
    std::optional<std::string> pendingCommand;
#endif

	// Graphics Pipeline
    GraphicsPipeline primitivePipeline;

    // Temporary stuffs
    Microsoft::WRL::ComPtr<ID3D11Buffer> vertexBuffer;
    Microsoft::WRL::ComPtr<ID3D11Buffer> objectConstantBuffer;
    EditorCamera camera; // Move this to a viewport later
};

EditorProgram::EditorProgram() = default;
EditorProgram::~EditorProgram() = default;

bool EditorProgram::Initialize(EngineServices& services)
{
	impl_ = std::make_unique<Impl>();
	impl_->window = &services.mainWindow;
    impl_->renderer = &services.renderer;

#if GILGAMESH_ENABLE_DEVELOPER_TOOLS
	impl_->consoleBuffer = std::make_shared<ConsoleBuffer>(1024);
	impl_->consoleSink = std::make_shared<ConsoleLogSink>(impl_->consoleBuffer);
	impl_->consoleRegistration = Logger::AddSink(impl_->consoleSink); 

    impl_->outputLogPanel =
        std::make_unique<OutputLogPanel>(
            impl_->consoleBuffer,
            impl_->consoleConfiguration);

    impl_->commandOutput =
        std::make_unique<ConsoleCommandOutput>(
            impl_->consoleBuffer);

    impl_->commandRegistry =
        CreateBuiltInCommandRegistry(
            impl_->consoleConfiguration);

    if (!impl_->imgui.Initialize(
        services.mainWindow,
        services.renderer.GetDevice(),
        services.renderer.GetDeviceContext()))
    {
        impl_.reset();
        return false;
    }

    // Construct panel and command objects.
#endif 

    if (!InitializePrimitiveTestResources(services.renderer))
    {
        impl_.reset();
        return false;
    }

    return true;
}

void EditorProgram::Update(const FrameContext& frame)
{
	impl_->camera.Update();

#if GILGAMESH_ENABLE_DEVELOPER_TOOLS
	// Process any pending console command from the previous frame before
    if (impl_->pendingCommand)
    {
        impl_->commandOutput->WriteCommand(
            *impl_->pendingCommand);

        const CommandResult result =
            impl_->commandRegistry.Execute(
                *impl_->commandOutput,
                *impl_->pendingCommand);

        impl_->commandOutput->WriteResult(result);
        impl_->pendingCommand.reset();
    }

    impl_->imgui.BeginFrame();
#endif
}

void EditorProgram::Render(RenderContext& context)
{
    Renderer& renderer = context.renderer;
    ID3D11DeviceContext* deviceContext =
        renderer.GetDeviceContext();

    const UINT stride = sizeof(SimpleVertex3D);
    const UINT offset = 0;

    impl_->primitivePipeline.Bind(deviceContext);

    ID3D11Buffer* vertexBuffer =
        impl_->vertexBuffer.Get();

    deviceContext->IASetVertexBuffers(
        0,
        1,
        &vertexBuffer,
        &stride,
        &offset);

    const float aspectRatio =
        static_cast<float>(context.outputExtent.width) /
        static_cast<float>(
            context.outputExtent.height > 0
                ? context.outputExtent.height
                : 1u);

    const DirectX::XMMATRIX model = DirectX::XMMatrixIdentity();
    const DirectX::XMMATRIX view = impl_->camera.GetViewMatrix();
    const DirectX::XMMATRIX projection =
        impl_->camera.GetProjectionMatrix(aspectRatio);

    ObjectConstants constants{};
    DirectX::XMStoreFloat4x4(
        &constants.modelViewProjection,
        DirectX::XMMatrixTranspose(model * view * projection));

    deviceContext->UpdateSubresource(
        impl_->objectConstantBuffer.Get(),
        0,
        nullptr,
        &constants,
        0,
        0);

    ID3D11Buffer* objectConstantBuffer =
        impl_->objectConstantBuffer.Get();
    deviceContext->VSSetConstantBuffers(
        0,
        1,
        &objectConstantBuffer);

    deviceContext->Draw(3, 0);

#if GILGAMESH_ENABLE_DEVELOPER_TOOLS
    impl_->workspace.DrawDockSpace();

    if (auto command = impl_->outputLogPanel->Draw())
        impl_->pendingCommand = std::move(*command);

    impl_->imgui.Render();
#endif
}

void EditorProgram::Shutdown()
{
    impl_.reset();
}

bool EditorProgram::InitializePrimitiveTestResources(Renderer& renderer)
{
	// Create a simple triangle vertex buffer for testing.
    SimpleVertex3D vertices[] = {
        { 0.0f, -0.5f, -0.5f, 1, 0, 0 },
        { 0.0f,  0.0f,  0.5f, 0, 1, 0 },
        { 0.0f,  0.5f, -0.5f, 0, 0, 1 },
    };
	D3D11_BUFFER_DESC bufferDesc{};
	bufferDesc.ByteWidth = sizeof(vertices);
	bufferDesc.Usage = D3D11_USAGE_DEFAULT;
	bufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	D3D11_SUBRESOURCE_DATA initData{};
	initData.pSysMem = vertices;
	HRESULT hr = renderer.GetDevice()->CreateBuffer(
		&bufferDesc,
		&initData,
		impl_->vertexBuffer.GetAddressOf());
	if (FAILED(hr))
	{
		GILGAMESH_LOG(Core, Error, "Failed to create vertex buffer: HRESULT=0x{:X}", hr);
		return false;
	}

    D3D11_BUFFER_DESC constantBufferDesc{};
    constantBufferDesc.ByteWidth = sizeof(ObjectConstants);
    constantBufferDesc.Usage = D3D11_USAGE_DEFAULT;
    constantBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

    hr = renderer.GetDevice()->CreateBuffer(
        &constantBufferDesc,
        nullptr,
        impl_->objectConstantBuffer.GetAddressOf());
    if (FAILED(hr))
    {
        GILGAMESH_LOG(Core, Error, "Failed to create object constant buffer: HRESULT=0x{:X}", hr);
        return false;
    }

	// Initialize the primitive graphics pipeline.
    // Shader Manager test. Move this to graphics pipeline later on.
    ShaderManager& shaders = renderer.GetShaderManager();
    const auto vsLoad = shaders.LoadVertex(L"Primitive");
    if (!vsLoad) return false;

    const auto psLoad = shaders.LoadPixel(L"Primitive");
    if (!psLoad) return false;

    const VertexShaderHandle vsHandle = vsLoad.resource;
    const PixelShaderHandle  psHandle = psLoad.resource;

    const D3D11_INPUT_ELEMENT_DESC inputElements[] =
    {
        {
            "POSITION",
            0,
            DXGI_FORMAT_R32G32B32_FLOAT,
            0,
            0,
            D3D11_INPUT_PER_VERTEX_DATA,
            0
        },
        {
            "COLOR",
            0,
            DXGI_FORMAT_R32G32B32_FLOAT,
            0,
            D3D11_APPEND_ALIGNED_ELEMENT,
            D3D11_INPUT_PER_VERTEX_DATA,
            0
        }
    };

    GraphicsPipelineDesc pipelineDesc{
        .vertexShader = vsLoad.resource,
        .pixelShader = psLoad.resource,
        .inputElements = inputElements,
        .topology =
            D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST
    };

    const HRESULT result =
        impl_->primitivePipeline.Initialize(
            renderer.GetDevice(),
            shaders,
            pipelineDesc);

    return SUCCEEDED(result);
}
