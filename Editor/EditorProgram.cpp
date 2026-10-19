#include "EditorProgram.h"

#include "EditorCamera/EditorCamera.h"
#include "EditorUI/Panels/EditorViewportPanel.h"
#include "EditorUI/Runtime/ImGuiIntegration.h"
#include "EditorUI/Workspace/EditorWorkspace.h"
#include "EditorViewport/EditorViewport.h"
#include "EditorViewport/EditorViewportClient.h"
#include "Editor/Render/EditorGridPass.h"
#include "Engine/Core/Logging/Logger.h"
#include "Engine/ECS/Component/MeshComponent.h"
#include "Engine/ECS/Component/TransformComponent.h"
#include "Engine/ECS/Framework/World.h"
#include "Engine/ECS/System/RenderExtractionSystem.h"
#include "Engine/Input/InputFrame.h"
#include "Engine/Input/InputSystem.h"
#include "Engine/Render/Mesh/PrimitiveMeshes.h"
#include "Engine/Render/RenderGraph/RenderGraph.h"
#include "Engine/Render/Renderer/RenderView.h"
#include "Engine/Render/Renderer/Renderer.h"
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
    #include "Editor/Console/ConsoleBuffer.h"
    #include "Editor/Console/ConsoleCommandOutput.h"
    #include "Editor/Console/ConsoleConfiguration.h"
    #include "Editor/Console/ConsoleLogSink.h"
    #include "Editor/EditorUI/Panels/OutputLogPanel.h"
#endif

// Owns the editor world and keeps its presentation resources within renderer lifetime.
struct EditorProgram::Impl
{
public:
    // Release captured submissions before removing the editor's demonstration geometry.
    ~Impl() noexcept;

public:
    // Core
    Window* window = nullptr;
    Renderer* renderer = nullptr;

    // Viewport
    EditorViewportClient viewportClient;
    EditorGridPass gridPass; // Editor-only overlay state and view/settings constant buffers.
    RenderGraph renderGraph;

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
    renderGraph.Reset();
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

    // The grid is an editor overlay and shares only camera data with scene rendering.
    if (!impl_->gridPass.Initialize(
        services.renderer.GetDevice(), services.renderer.GetShaderManager()))
    {
        GILGAMESH_LOG(Renderer, Error, "Failed to initialize the editor grid pass");
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
    impl_->demoCubeMesh = meshes.CreateMesh(PrimitiveMeshes::MakeUnitCubeMeshData());
    if (!meshes.IsValid(impl_->demoCubeMesh))
    {
        GILGAMESH_LOG(Core, Error,
            "Failed to initialize editor scene geometry");
        impl_.reset();
        return false;
    }

    const SceneHandle scene = impl_->world.RequestCreateScene("EditorScene");
    const Entity cube = impl_->world.CreateEntity(scene);
    impl_->world.Emplace<TransformComponent>(cube);
    impl_->world.Emplace<MeshComponent>(cube, MeshComponent{ impl_->demoCubeMesh });
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
            importedDepth.readOnlyDepthStencilView =
                viewport.GetReadOnlyDepthStencilView();

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
                .projection = camera.GetProjectionMatrix(aspectRatio),
                .cameraWorldPos = camera.GetPosition(),
                .extent = viewportFrame.renderExtent
            };

            // The renderer prepares pass-local commands and the graph owns their captured values.
            if (!renderer.AddScenePasses(renderGraph, colorTarget, depthTarget, *frame, renderView))
            {
                throw std::runtime_error("Failed to prepare editor scene draw commands");
            }

            // Load scene color/depth and blend the world grid without changing mesh depth.
            if (!impl_->gridPass.AddToGraph(renderGraph, colorTarget, depthTarget, renderView))
            {
                throw std::runtime_error("Failed to prepare the editor grid pass");
            }

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
        catch (const std::exception& error)
        {
            // A failed presentation must not leave an older snapshot or partial graph for reuse.
            impl_->extractionSystem->DiscardFrame();
            renderGraph.Reset();
            GILGAMESH_LOG(Renderer, Error, "Editor scene rendering failed: {}", error.what());
        }
        catch (...)
        {
            impl_->extractionSystem->DiscardFrame();
            renderGraph.Reset();
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
    // Impl owns both graph cleanup and the demonstration mesh's resource retirement.
    impl_.reset();
}
