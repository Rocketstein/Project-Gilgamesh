#pragma once

#include <memory>
#include <vector>

#include <d3d11.h>

#include "Engine/Render/Passes/IRenderPass.h"
#include "Engine/Render/RenderGraph/RenderGraph.h"

class EditorViewport;
class Renderer;

// Owns an editor viewport pipeline and releases its graph before returning each frame.
class EditorViewportRenderer final
{
public:
    EditorViewportRenderer() = default;
    ~EditorViewportRenderer() = default;

    EditorViewportRenderer(const EditorViewportRenderer&) = delete;
    EditorViewportRenderer& operator=(const EditorViewportRenderer&) = delete;
    EditorViewportRenderer(EditorViewportRenderer&&) = delete;
    EditorViewportRenderer& operator=(EditorViewportRenderer&&) = delete;

    // Initializes a replacement pipeline without changing the current one on failure.
    // Borrowed services must outlive this module and must not be replaced during Render().
    // Initialize() and Render() reject calls made from this module's active pass callbacks.
    [[nodiscard]]
    bool Initialize(
        ID3D11Device* device,
        ID3D11DeviceContext* context,
        ShaderManager& shaders,
        const MeshManager& meshes,
        std::vector<std::unique_ptr<IRenderPass>> passes);

    // Uses the engine renderer's services without taking ownership of the renderer.
    [[nodiscard]]
    bool Initialize(
        Renderer& renderer,
        std::vector<std::unique_ptr<IRenderPass>> passes);

    // Executes synchronously; imported targets and copied submissions are released before return.
    [[nodiscard]]
    bool Render(
        const EditorViewport& viewport,
        const RenderFrameData& frame,
        const RenderView& view);

    [[nodiscard]]
    bool IsInitialized() const noexcept;

private:
    // Imports the current viewport targets and registers the configured passes in order.
    [[nodiscard]]
    bool BuildAndExecuteGraph(
        const EditorViewport& viewport,
        const RenderFrameData& frame,
        const RenderView& view);

private:
    ID3D11Device* device_ = nullptr; // Borrowed device used to realize the graph.
    ID3D11DeviceContext* context_ = nullptr; // Borrowed immediate context used for execution.
    const MeshManager* meshes_ = nullptr; // Borrowed resource registry used by mesh passes.
    bool busy_ = false; // Prevents pass callbacks from replacing or resetting their active pipeline.
    std::vector<std::unique_ptr<IRenderPass>> passes_; // Owns pass resources and their registration order.
    RenderGraph graph_; // Declared last so captured callbacks are destroyed before their passes.
};
