#include "EditorViewportRenderer.h"

#include <exception>
#include <string_view>
#include <utility>

#include "Editor/EditorViewport/EditorViewport.h"
#include "Engine/Core/Logging/Logger.h"
#include "Engine/Render/Renderer/RenderView.h"
#include "Engine/Render/Renderer/Renderer.h"

namespace
{
    // Derives the graph description from the native resource being imported.
    RenderGraphTextureHandle ImportViewportTexture(
        RenderGraph& graph,
        std::string_view name,
        const ImportedRenderGraphTexture& imported)
    {
        D3D11_TEXTURE2D_DESC nativeDescription{};
        imported.texture->GetDesc(&nativeDescription);

        const RenderGraphTextureDesc description{
            .extent = { nativeDescription.Width, nativeDescription.Height },
            .format = nativeDescription.Format,
            .mipLevels = nativeDescription.MipLevels,
            .sampleCount = nativeDescription.SampleDesc.Count
        };
        return graph.ImportTexture(name, description, imported);
    }
} // Anonymous namespace

bool EditorViewportRenderer::Initialize(
    ID3D11Device* device,
    ID3D11DeviceContext* context,
    ShaderManager& shaders,
    const MeshManager& meshes,
    std::vector<std::unique_ptr<IRenderPass>> passes)
{
    // Pass initialization and execution callbacks must not replace their own active pipeline.
    if (busy_)
    {
        GILGAMESH_LOG(Renderer, Error,
            "Cannot reenter editor viewport pipeline initialization or rendering");
        return false;
    }

    // A complete pipeline requires live renderer services and at least one pass.
    if (!device || !context || !shaders.IsInitialized() || !meshes.IsInitialized() || passes.empty())
    {
        GILGAMESH_LOG(Renderer, Error,
            "Cannot initialize an editor viewport renderer without services and passes");
        return false;
    }

    // Synchronous execution requires the immediate context from the supplied device.
    Microsoft::WRL::ComPtr<ID3D11Device> contextDevice;
    context->GetDevice(contextDevice.GetAddressOf());
    if (contextDevice.Get() != device || context->GetType() != D3D11_DEVICE_CONTEXT_IMMEDIATE)
    {
        GILGAMESH_LOG(Renderer, Error,
            "Editor viewport rendering requires an immediate context from its device");
        return false;
    }

    // Build resources in the incoming list so a failed replacement preserves the old pipeline.
    busy_ = true;
    bool initialized = true;
    try
    {
        for (const auto& pass : passes)
        {
            if (!pass || !pass->Initialize(device, shaders))
            {
                initialized = false;
                break;
            }
        }

        // Release callbacks before retiring old pass resources, then publish the replacement.
        if (initialized)
        {
            graph_.Reset();
            passes_ = std::move(passes);
            device_ = device;
            context_ = context;
            meshes_ = &meshes;
        }
    }
    catch (const std::exception& error)
    {
        busy_ = false;
        GILGAMESH_LOG(Renderer, Error,
            "Editor viewport pipeline initialization failed: {}", error.what());
        return false;
    }
    catch (...)
    {
        busy_ = false;
        GILGAMESH_LOG(Renderer, Error,
            "Editor viewport pipeline initialization failed with an unknown exception");
        return false;
    }

    busy_ = false;
    if (!initialized)
    {
        GILGAMESH_LOG(Renderer, Error,
            "Failed to initialize an editor viewport render pass");
    }
    return initialized;
}

bool EditorViewportRenderer::Initialize(
    Renderer& renderer,
    std::vector<std::unique_ptr<IRenderPass>> passes)
{
    return Initialize(
        renderer.GetDevice(),
        renderer.GetDeviceContext(),
        renderer.GetShaderManager(),
        renderer.GetMeshManager(),
        std::move(passes));
}

bool EditorViewportRenderer::IsInitialized() const noexcept
{
    return device_ && context_ && meshes_ && !passes_.empty();
}

bool EditorViewportRenderer::Render(
    const EditorViewport& viewport,
    const RenderFrameData& frame,
    const RenderView& view)
{
    // Reject nested callbacks before they can reset a graph that is currently executing.
    if (busy_)
    {
        GILGAMESH_LOG(Renderer, Error,
            "Cannot reenter editor viewport pipeline initialization or rendering");
        return false;
    }

    // No graph-owned callback or imported pointer may survive a submitted viewport frame.
    busy_ = true;
    graph_.Reset();
    bool rendered = false;
    try
    {
        rendered = BuildAndExecuteGraph(viewport, frame, view);
    }
    catch (const std::exception& error)
    {
        graph_.Reset();
        busy_ = false;
        GILGAMESH_LOG(Renderer, Error,
            "Editor viewport rendering failed: {}", error.what());
        return false;
    }
    catch (...)
    {
        graph_.Reset();
        busy_ = false;
        GILGAMESH_LOG(Renderer, Error,
            "Editor viewport rendering failed with an unknown exception");
        return false;
    }

    // The next UI frame may now resize or destroy the viewport immediately.
    graph_.Reset();
    busy_ = false;
    return rendered;
}

bool EditorViewportRenderer::BuildAndExecuteGraph(
    const EditorViewport& viewport,
    const RenderFrameData& frame,
    const RenderView& view)
{
    // Reject incomplete or mismatched inputs before importing any viewport resources.
    if (!IsInitialized() || !viewport.IsReady() || view.extent != viewport.GetExtent())
    {
        GILGAMESH_LOG(Renderer, Error,
            "Cannot render an uninitialized viewport pipeline or mismatched viewport extent");
        return false;
    }

    // The viewport owns these resources; synchronous graph execution only borrows them.
    const ImportedRenderGraphTexture importedColor{
        .texture = viewport.GetColorTexture(),
        .renderTargetView = viewport.GetColorRenderTargetView(),
        .shaderResourceView = viewport.GetColorShaderResourceView()
    };
    const ImportedRenderGraphTexture importedDepth{
        .texture = viewport.GetDepthTexture(),
        .writableDepthStencilView = viewport.GetDepthStencilView(),
        .readOnlyDepthStencilView = viewport.GetReadOnlyDepthStencilView()
    };
    const RenderGraphTextureHandle color =
        ImportViewportTexture(graph_, "EditorViewport.Color", importedColor);
    const RenderGraphTextureHandle depth =
        ImportViewportTexture(graph_, "EditorViewport.Depth", importedDepth);

    // Concrete pass selection belongs to the caller; registration preserves that policy.
    const RenderPassContext passContext{ color, depth, frame, view, *meshes_ };
    for (const auto& pass : passes_)
    {
        if (!pass->AddToGraph(graph_, passContext))
        {
            GILGAMESH_LOG(Renderer, Error,
                "Failed to prepare an editor viewport render pass");
            return false;
        }
    }

    // Compile once after all declarations, then execute against the borrowed immediate context.
    if (!graph_.Compile(device_) || !graph_.Execute(context_))
    {
        GILGAMESH_LOG(Renderer, Error,
            "Failed to execute the editor viewport render graph");
        return false;
    }
    return true;
}
