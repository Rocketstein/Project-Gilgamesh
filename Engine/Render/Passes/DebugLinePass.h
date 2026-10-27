#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include "Engine/Render/Buffers/ConstantBuffer.h"
#include "Engine/Render/Buffers/VertexBuffer.h"
#include "Engine/Render/DrawSetup/DrawSetup.h"
#include "Engine/Render/Mesh/Vertex.h"
#include "Engine/Render/Renderer/RenderFrameData.h"
#include "Engine/Render/Renderer/RenderView.h"
#include "Engine/Render/Shader/ShaderConstants.h"
#include "IRenderPass.h"

// Owns the camera constants and expanded line vertices for one view's submission.
struct DebugLinePassCommands
{
    FrameConstants view{}; // Transposed camera data, uploaded only when this submission executes.
    std::vector<SimpleVertex3D> vertices; // Two world-space RGB vertices per independent line.
};

// Batches world-space debug lines using read-only scene depth and no depth writes.
class DebugLinePass final : public IRenderPass
{
public:
    // Publish shaders and fixed state together; vertex storage is allocated lazily.
    [[nodiscard]] bool Initialize(ID3D11Device* device, ShaderManager& shaders) override;

    // Capture owned commands and load the color/depth produced by preceding scene passes.
    [[nodiscard]] bool AddToGraph(RenderGraph& graph, const RenderPassContext& context) const override;

    // Expand segments into vertex pairs without allocating or updating GPU resources.
    [[nodiscard]] std::optional<DebugLinePassCommands> PrepareCommands(
        const RenderFrameData& frame, const RenderView& view) const;

    // Upload this view's commands and draw exactly their active vertex count.
    [[nodiscard]] bool ExecuteCommands(
        ID3D11DeviceContext* context, const DebugLinePassCommands& commands) const;

    [[nodiscard]] bool IsInitialized() const noexcept;

private:
    // Grow storage for a positive, validated count while preserving the old buffer on failure.
    [[nodiscard]] bool EnsureVertexCapacity(std::uint32_t requiredVertexCount) const;

private:
    DrawSetup drawSetup_; // RGB line shaders with LESS_EQUAL depth testing and depth writes disabled.
    ConstantBuffer<FrameConstants> viewCB_; // Reused at execution, independently for every prepared view.
    Microsoft::WRL::ComPtr<ID3D11Device> device_; // Retained for later dynamic buffer growth.
    mutable VertexBuffer vertexBuffer_; // GPU cache; GetCount reports capacity rather than draw size.
};
