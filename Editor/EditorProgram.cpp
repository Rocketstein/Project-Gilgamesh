#include "EditorProgram.h"

#include "EditorUI/Panels/EditorViewportPanel.h"
#include "EditorUI/Runtime/ImGuiIntegration.h"
#include "EditorUI/Workspace/EditorWorkspace.h"
#include "EditorViewport/EditorViewportClient.h"
#include "Editor/Render/EditorViewportPasses.h"
#include "Editor/Render/EditorViewportRenderer.h"
#include "Engine/Core/Logging/Logger.h"
#include "Engine/ECS/Component/MeshComponent.h"
#include "Engine/ECS/Component/TransformComponent.h"
#include "Engine/ECS/Framework/World.h"

#include "Engine/ECS/Component/BoundsComponent.h"
#include "Engine/ECS/System/Transform/TransformHierarchySystem.h"
#include "Engine/ECS/System/Transform/LocalTransformSystem.h"
#include "Engine/ECS/System/Transform/LocalToWorldSystem.h"
#include "Engine/ECS/System/Transform/UpdateBoundsSystem.h"
#include "Engine/ECS/System/RenderExtractionSystem.h"

#include "Engine/Input/InputSystem.h"
#include "Engine/Render/Mesh/PrimitiveMeshes.h"
#include "Engine/Render/Renderer/RenderView.h"
#include "Engine/Runtime/EngineServices.h"
#include "Engine/Runtime/FrameContext.h"
#include "Engine/Runtime/RenderContext.h"

#include <exception>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>

#if GILGAMESH_ENABLE_EDITOR_DIAGNOSTICS
    #include "Editor/Console/Commands/BuiltInCommands.h"
    #include "Editor/Console/ConsoleCommandOutput.h"
    #include "Editor/Console/ConsoleConfiguration.h"
    #include "Editor/Console/ConsoleLogSink.h"
    #include "Editor/EditorUI/Panels/OutputLogPanel.h"
#endif

// Owns the editor world and keeps its presentation resources within renderer lifetime.
struct EditorProgram::Impl
{
public:
    // Remove demonstration geometry while the borrowed renderer services still exist.
    ~Impl() noexcept;

public:
    // Core
    Window* window = nullptr;
    Renderer* renderer = nullptr;

    // Viewport
    EditorViewportClient viewportClient;
    EditorViewportRenderer viewportRenderer; // Owns the configured passes and synchronous graph submission.

    // Persistent scene state belongs to the world; the group owns the extractor.
    World world;
    RenderExtractionSystem* extractionSystem = nullptr; // Borrowed until world destruction.
    MeshHandle demoCubeMesh; // Editor-owned resource used by the demonstration entity.
    FrameContext frame{}; // Latest application timing used by presentation extraction.

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

// Renderer services outlive the program, including partially initialized editor state.
EditorProgram::Impl::~Impl() noexcept
{
    if (!renderer || !demoCubeMesh) return;

    // Resource retirement can allocate its free-list entry; teardown must not throw.
    try
    {
        (void)renderer->GetMeshManager().RemoveMesh(demoCubeMesh);
    }
    catch (...)
    {
        GILGAMESH_LOG(Renderer, Error, "Failed to release the editor demonstration mesh");
    }
}

EditorProgram::EditorProgram() = default;
EditorProgram::~EditorProgram() = default;

// Compose the editor UI and the world-backed demonstration scene.
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

    // The viewport module owns GPU pass resources; the factory selects editor drawing order.
    if (!impl_->viewportRenderer.Initialize(services.renderer, CreateEditorViewportPasses()))
    {
        GILGAMESH_LOG(Renderer, Error, "Failed to initialize the editor viewport renderer");
        impl_.reset();
        return false;
    }

    // Register the world-owned extractor before creating any presentation content.
    auto extractionSystem = std::make_unique<RenderExtractionSystem>();
    RenderExtractionSystem* extractor = extractionSystem.get();
    impl_->world.GetExtractionSystems().AddSystem(std::move(extractionSystem));
    extractor->SetEnabled(true);
    impl_->extractionSystem = extractor;

    // Build the temporary demonstration through the same ECS path as ordinary scene content.
    MeshManager& meshes = services.renderer.GetMeshManager();
    SystemGroup& transforms = impl_->world.GetTransformSystems();

    // Keep the typed reference before transferring ownership to the group.
    auto hierarchySystem = std::make_unique<TransformHierarchySystem>();
    TransformHierarchySystem& hierarchy = *hierarchySystem;

    // Registration order determines evaluation order.
    transforms.AddSystem(std::move(hierarchySystem))
        .SetEnabled(true);

    transforms.AddSystem(std::make_unique<LocalTransformSystem>())
        .SetEnabled(true);

    transforms.AddSystem(std::make_unique<LocalToWorldSystem>(hierarchy))
        .SetEnabled(true);

    transforms.AddSystem(std::make_unique<UpdateBoundsSystem>(meshes))
        .SetEnabled(true);

    impl_->demoCubeMesh = meshes.CreateMesh(PrimitiveMeshes::MakeUnitCubeMeshData());
    if (!meshes.IsValid(impl_->demoCubeMesh))
    {
        GILGAMESH_LOG(Core, Error,
            "Failed to initialize editor scene geometry");
        impl_.reset();
        return false;
    }

    const SceneHandle scene = impl_->world.RequestCreateScene("EditorScene");

    // Queue all presentation components before the first playback.
    const Entity cube = impl_->world.CreateEntity(scene);
    impl_->world.Emplace<TransformComponent>(cube);
    impl_->world.Emplace<MeshComponent>(cube, MeshComponent{ impl_->demoCubeMesh });
    impl_->world.Emplace<AABBComponent>(cube);
    if (!impl_->world.RequestSetActiveScene(scene))
    {
        GILGAMESH_LOG(Core, Error, "Failed to select the editor scene");
        impl_.reset();
        return false;
    }

    // Receive input system reference from EngineServices
	impl_->inputSystem = &services.input;

    return true;
}

// Advance input and simulation before the UI records this frame's editor requests.
void EditorProgram::Update(const FrameContext& frame)
{
    impl_->frame = frame;
    const InputFrame& inputFrame = impl_->inputSystem->GetFrame();
	impl_->viewportClient.Update(inputFrame, frame.realDeltaTime);

#if GILGAMESH_ENABLE_EDITOR_DIAGNOSTICS
	// Execute the previous frame's console request before simulation playback.
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

    // Simulation playback is independent from the later editor presentation boundary.
    impl_->world.Update(frame.realDeltaTime);
    impl_->imgui.BeginFrame();
}

// Extract the finalized world and submit its copied data to the viewport graph.
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
        try
        {
            // Include the latest editor requests before extracting one owned scene snapshot.
            impl_->world.PreparePresentation(impl_->frame.realDeltaTime);
            auto frame = impl_->extractionSystem->TakeFrame();
            if (!frame)
            {
                throw std::runtime_error("Editor render extraction produced no frame");
            }
            frame->frameNumber = impl_->frame.frameNumber;

            const float aspectRatio =
                static_cast<float>(viewportFrame.renderExtent.width) /
                static_cast<float>(
                    viewportFrame.renderExtent.height > 0
                        ? viewportFrame.renderExtent.height
                        : 1u);

            const auto& camera = impl_->viewportClient.GetEditorCamera();
            const RenderView renderView{
                .view = camera.GetViewMatrix(),
                .projection = camera.GetProjectionMatrix(aspectRatio),
                .cameraWorldPos = camera.GetPosition(),
                .extent = viewportFrame.renderExtent
            };

            // The module logs submission failures and releases every captured resource before return.
            (void)impl_->viewportRenderer.Render(viewport, *frame, renderView);
        }
        catch (const std::exception& error)
        {
            // A failed presentation must not leave an older extraction result for reuse.
            impl_->extractionSystem->DiscardFrame();
            GILGAMESH_LOG(Renderer, Error, "Editor scene rendering failed: {}", error.what());
        }
        catch (...)
        {
            impl_->extractionSystem->DiscardFrame();
            GILGAMESH_LOG(Renderer, Error, "Editor scene rendering failed with an unknown exception");
        }
    }
    else
    {
        // Hidden viewports must not retain a result that could look like fresh extraction later.
        impl_->extractionSystem->DiscardFrame();
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
    // Impl retires demonstration geometry and destroys the viewport pipeline within renderer lifetime.
    impl_.reset();
}
