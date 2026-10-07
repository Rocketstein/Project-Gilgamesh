#include "EditorProgram.h"

#include "EditorCamera/EditorCamera.h"
#include "EditorScene/EditorPreviewScene.h"
#include "EditorUI/Panels/EditorViewportPanel.h"
#include "EditorUI/Runtime/ImGuiIntegration.h"
#include "EditorUI/Workspace/EditorWorkspace.h"
#include "EditorViewport/EditorViewport.h"
#include "EditorViewport/EditorViewportClient.h"
#include "Engine/Core/Color.h"
#include "Engine/Core/Logging/Logger.h"
#include "Engine/Input/InputFrame.h"
#include "Engine/Input/InputSystem.h"
#include "Engine/Render/RenderGraph/RenderGraph.h"
#include "Engine/Render/Renderer/RenderView.h"
#include "Engine/Render/Renderer/Renderer.h"
#include "Engine/Render/Renderer/StaticMeshDrawItem.h"
#include "Engine/Runtime/EngineServices.h"
#include "Engine/Runtime/FrameContext.h"
#include "Engine/Runtime/RenderContext.h"

#include <optional>
#include <string>
#include <utility>
#include <vector>

#if GILGAMESH_ENABLE_EDITOR_DIAGNOSTICS
    #include "Editor/Console/Commands/BuiltInCommands.h"
    #include "Editor/Console/ConsoleBuffer.h"
    #include "Editor/Console/ConsoleCommandOutput.h"
    #include "Editor/Console/ConsoleConfiguration.h"
    #include "Editor/Console/ConsoleLogSink.h"
    #include "Editor/EditorUI/Panels/OutputLogPanel.h"
#endif

struct EditorProgram::Impl
{
    // Core
    Window* window = nullptr;
    Renderer* renderer = nullptr;

    // Viewport
    EditorViewportClient viewportClient;
    RenderGraph renderGraph;

    // Preview content
    EditorPreviewScene previewScene;

    // Editor UI
    ImGuiIntegration imgui;
    EditorWorkspace workspace;
    EditorViewportPanel viewportPanel;

#if GILGAMESH_ENABLE_EDITOR_DIAGNOSTICS
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

    // Editor Inputs
    const InputSystem* inputSystem = nullptr;
};

EditorProgram::EditorProgram() = default;
EditorProgram::~EditorProgram() = default;

bool EditorProgram::Initialize(EngineServices& services)
{
	impl_ = std::make_unique<Impl>();
	impl_->window = &services.mainWindow;
    impl_->renderer = &services.renderer;

#if GILGAMESH_ENABLE_EDITOR_DIAGNOSTICS
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
#endif

    if (!impl_->imgui.Initialize(
        services.mainWindow,
        services.renderer.GetDevice(),
        services.renderer.GetDeviceContext()))
    {
        impl_.reset();
        return false;
    }

    // Initialize editor viewport
    impl_->viewportClient.Initialize(services.renderer.GetDevice());

    if (!impl_->previewScene.Initialize(
        services.renderer.GetMeshManager()))
    {
        GILGAMESH_LOG(Core, Error,
            "Failed to initialize editor preview scene");
        impl_.reset();
        return false;
    }

    // Receive input system reference from EngineServices
	impl_->inputSystem = &services.input;

    return true;
}

void EditorProgram::Update(const FrameContext& frame)
{
    const InputFrame& inputFrame = impl_->inputSystem->GetFrame();
	impl_->viewportClient.Update(inputFrame, frame.realDeltaTime);

#if GILGAMESH_ENABLE_EDITOR_DIAGNOSTICS
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
#endif

    impl_->imgui.BeginFrame();
}

void EditorProgram::Render(RenderContext& context)
{
    Renderer& renderer = context.renderer;
    RenderGraph& renderGraph = impl_->renderGraph;

    // Drop last frame's imported pointers before the viewport may resize.
    renderGraph.Reset();

    impl_->workspace.DrawDockSpace();

    EditorViewport& viewport =
        impl_->viewportClient.GetEditorViewport();

    const EditorViewportPanelFrame viewportFrame =
        impl_->viewportPanel.Draw(viewport);

    if (viewportFrame.shouldRender)
    {
        ID3D11Texture2D* colorTexture =
            viewport.GetColorTexture();
        ID3D11Texture2D* depthTexture =
            viewport.GetDepthTexture();

        // Describe and import the viewport-owned native resources. The graph
        // borrows these objects only until its next Reset().
        D3D11_TEXTURE2D_DESC colorNativeDescription{};
        D3D11_TEXTURE2D_DESC depthNativeDescription{};
        colorTexture->GetDesc(&colorNativeDescription);
        depthTexture->GetDesc(&depthNativeDescription);

        RenderGraphTextureDesc colorDescription;
        colorDescription.extent = {
            colorNativeDescription.Width,
            colorNativeDescription.Height
        };
        colorDescription.format = colorNativeDescription.Format;
        colorDescription.mipLevels = colorNativeDescription.MipLevels;
        colorDescription.sampleCount =
            colorNativeDescription.SampleDesc.Count;

        ImportedRenderGraphTexture importedColor;
        importedColor.texture = colorTexture;
        importedColor.renderTargetView =
            viewport.GetColorRenderTargetView();
        importedColor.shaderResourceView =
            viewport.GetColorShaderResourceView();

        RenderGraphTextureDesc depthDescription;
        depthDescription.extent = {
            depthNativeDescription.Width,
            depthNativeDescription.Height
        };
        depthDescription.format = depthNativeDescription.Format;
        depthDescription.mipLevels = depthNativeDescription.MipLevels;
        depthDescription.sampleCount =
            depthNativeDescription.SampleDesc.Count;

        ImportedRenderGraphTexture importedDepth;
        importedDepth.texture = depthTexture;
        importedDepth.writableDepthStencilView =
            viewport.GetDepthStencilView();

        const RenderGraphTextureHandle colorTarget =
            renderGraph.ImportTexture(
                "EditorViewport.Color",
                colorDescription,
                importedColor);
        const RenderGraphTextureHandle depthTarget =
            renderGraph.ImportTexture(
                "EditorViewport.Depth",
                depthDescription,
                importedDepth);

        const float aspectRatio =
            static_cast<float>(viewportFrame.renderExtent.width) /
            static_cast<float>(
                viewportFrame.renderExtent.height > 0
                    ? viewportFrame.renderExtent.height
                    : 1u);

        const auto& camera = impl_->viewportClient.GetEditorCamera();
        const RenderView renderView{
            .view = camera.GetViewMatrix(),
            .projection = camera.GetProjectionMatrix(aspectRatio)
        };

        std::vector<StaticMeshDrawItem> drawItems;
        impl_->previewScene.GatherDrawItems(
            renderer.GetMeshManager(),
            drawItems);

        // The graph owns pass ordering and attachment state; the technique
        // remains responsible for draw-local bindings and draw submission.
        renderGraph.AddPass(
            "EditorScene",
            [colorTarget, depthTarget](GraphPassBuilder& builder)
            {
                builder.SetColorTarget(
                    0,
                    colorTarget,
                    RenderGraphLoadOp::Clear,
                    Color4{ 0.1f, 0.12f, 0.16f });
                builder.SetDepthTarget(
                    depthTarget,
                    RenderGraphDepthAccess::ReadWrite,
                    RenderGraphLoadOp::Clear,
                    1.0f);
            },
            [
                &renderer,
                renderView,
                drawItems = std::move(drawItems)
            ](ID3D11DeviceContext& deviceContext)
            {
                renderer.GetSimpleMeshRenderer().Draw(
                    &deviceContext,
                    renderView,
                    drawItems);
            });

        const bool compiled =
            renderGraph.Compile(renderer.GetDevice());
        const bool executed = compiled
            && renderGraph.Execute(renderer.GetDeviceContext());
        if (!executed)
        {
            GILGAMESH_LOG(
                Renderer,
                Error,
                "Failed to execute the editor scene render graph");
        }
    }

#if GILGAMESH_ENABLE_EDITOR_DIAGNOSTICS
    if (auto command = impl_->outputLogPanel->Draw())
    {
        impl_->pendingCommand = std::move(*command);
    }
#endif

    // RenderGraph leaves its attachments unbound; ImGui targets the swap chain.
    renderer.BindOutput();
    impl_->imgui.Render();
}

void EditorProgram::Shutdown()
{
    if (impl_ && impl_->renderer)
    {
        impl_->renderGraph.Reset();
        impl_->previewScene.Shutdown(
            impl_->renderer->GetMeshManager());
    }

    impl_.reset();
}
