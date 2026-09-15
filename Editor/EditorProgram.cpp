#include "EditorProgram.h"

#include "EditorCamera/EditorCamera.h"
#include "EditorUI/Panels/EditorViewportPanel.h"
#include "EditorUI/Runtime/ImGuiIntegration.h"
#include "EditorUI/Workspace/EditorWorkspace.h"
#include "EditorViewport/EditorViewport.h"
#include "EditorViewport/EditorViewportClient.h"
#include "Engine/Core/Logging/Logger.h"
#include "Engine/Input/InputFrame.h"
#include "Engine/Input/InputSystem.h"
#include "Engine/Render/Mesh/PrimitiveMeshes.h"
#include "Engine/Render/Renderer/RenderView.h"
#include "Engine/Render/Renderer/Renderer.h"
#include "Engine/Render/Renderer/StaticMeshDrawItem.h"
#include "Engine/Runtime/EngineServices.h"
#include "Engine/Runtime/FrameContext.h"
#include "Engine/Runtime/RenderContext.h"

#include <array>
#include <optional>
#include <string>
#include <utility>

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

    // Temporary cube resource
    MeshHandle cubeMesh;
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

    if (!InitializeCubeMesh(services.renderer))
    {
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

    impl_->workspace.DrawDockSpace();

    EditorViewport& viewport =
        impl_->viewportClient.GetEditorViewport();

    const EditorViewportPanelFrame viewportFrame =
        impl_->viewportPanel.Draw(viewport);

    if (viewportFrame.shouldRender)
    {
        ID3D11DeviceContext* deviceContext =
            renderer.GetDeviceContext();

        viewport.BindAndClear(deviceContext);

        const float aspectRatio =
            static_cast<float>(viewportFrame.renderExtent.width) /
            static_cast<float>(
                viewportFrame.renderExtent.height > 0
                    ? viewportFrame.renderExtent.height
                    : 1u);

        const auto camera = impl_->viewportClient.GetEditorCamera();
        const RenderView renderView{
            .view = camera.GetViewMatrix(),
            .projection = camera.GetProjectionMatrix(aspectRatio)
        };

        const std::array drawItems{
            StaticMeshDrawItem{
                .mesh = renderer.GetMeshManager().TryGet(impl_->cubeMesh),
                .localToWorld = Matrix4::Identity()
            }
        };

        renderer.GetSimpleMeshRenderer().Draw(
            deviceContext,
            renderView,
            drawItems);

        viewport.Unbind(deviceContext);

        // Restore the swap-chain back buffer without clearing it so the
        // queued ImGui draw data is submitted to the presentation output.
        renderer.BindOutput();
    }

#if GILGAMESH_ENABLE_EDITOR_DIAGNOSTICS
    if (auto command = impl_->outputLogPanel->Draw())
        impl_->pendingCommand = std::move(*command);
#endif

    impl_->imgui.Render();
}

void EditorProgram::Shutdown()
{
    if (impl_ && impl_->renderer)
    {
        (void)impl_->renderer->GetMeshManager().RemoveMesh(
            impl_->cubeMesh);
    }

    impl_.reset();
}

bool EditorProgram::InitializeCubeMesh(Renderer& renderer)
{
    MeshManager& meshes = renderer.GetMeshManager();
    impl_->cubeMesh = meshes.CreateMesh(
        PrimitiveMeshes::MakeUnitCubeMeshData());
    if (!meshes.IsValid(impl_->cubeMesh))
    {
        GILGAMESH_LOG(Core, Error, "Failed to create cube static mesh");
        return false;
    }

    return true;
}
