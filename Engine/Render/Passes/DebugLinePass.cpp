#include "DebugLinePass.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <utility>

#include "Engine/Core/Logging/Logger.h"
#include "Engine/Render/RenderGraph/RenderGraph.h"

namespace
{
// D3D11 buffer byte widths and draw vertex counts both use unsigned 32-bit values.
constexpr std::uint32_t MaxVertexCount =
    std::numeric_limits<std::uint32_t>::max() / static_cast<std::uint32_t>(sizeof(SimpleVertex3D));

// Reject undefined position and color arithmetic before recording GPU work.
bool IsFinite(const Vector3& value) noexcept
{
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}
} // Anonymous namespace

// Build replacement resources privately so an initialization failure preserves the current pass.
bool DebugLinePass::Initialize(ID3D11Device* device, ShaderManager& shaders)
{
    if (!device || !shaders.IsInitialized())
    {
        GILGAMESH_LOG(Renderer, Error, "Debug line pass requires a device and initialized shaders");
        return false;
    }
    const auto vertexShader = shaders.LoadVertex(L"DebugLine");
    const auto pixelShader = shaders.LoadPixel(L"DebugLine");
    if (!vertexShader || !pixelShader)
    {
        GILGAMESH_LOG(Renderer, Error, "Failed to load debug line shaders");
        return false;
    }

    // Reuse the existing compact position/RGB layout for every expanded segment.
    const D3D11_INPUT_ELEMENT_DESC inputElements[]{
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,
            static_cast<UINT>(offsetof(SimpleVertex3D, x)), D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,
            static_cast<UINT>(offsetof(SimpleVertex3D, r)), D3D11_INPUT_PER_VERTEX_DATA, 0 }
    };
    const DrawSetupDesc description{
        .vertexShader = vertexShader.resource,
        .pixelShader = pixelShader.resource,
        .inputElements = inputElements,
        .drawState = {
            .topology = D3D11_PRIMITIVE_TOPOLOGY_LINELIST,
            .cullMode = D3D11_CULL_NONE,
            .depthTestEnabled = true,
            .depthWriteEnabled = false,
            .depthComparison = D3D11_COMPARISON_LESS_EQUAL
        }
    };

    DrawSetup newDrawSetup;
    ConstantBuffer<FrameConstants> newViewBuffer;
    if (FAILED(newDrawSetup.Initialize(device, shaders, description)) || !newViewBuffer.Create(device))
    {
        GILGAMESH_LOG(Renderer, Error, "Failed to create debug line pass resources");
        return false;
    }

    // Reinitialization also drops vertex storage belonging to the previous device.
    device_ = device;
    drawSetup_ = std::move(newDrawSetup);
    viewCB_ = std::move(newViewBuffer);
    vertexBuffer_ = VertexBuffer{};
    return true;
}

// Keep CPU submission data independent of the frame and view passed by the caller.
bool DebugLinePass::AddToGraph(RenderGraph& graph, const RenderPassContext& context) const
{
    if (!IsInitialized() || !graph.IsValid(context.color) || !graph.IsValid(context.depth))
    {
        GILGAMESH_LOG(Renderer, Error, "Cannot add debug lines without initialized resources and valid targets");
        return false;
    }
    auto commands = PrepareCommands(context.frame, context.view);
    if (!commands) return false;
    if (commands->vertices.empty()) return true;

    // Read scene depth without changing it; color remains layered over preceding passes.
    graph.AddPass(
        "DebugLines",
        [color = context.color, depth = context.depth](GraphPassBuilder& builder)
        {
            builder.SetColorTarget(0, color, RenderGraphLoadOp::Load);
            builder.SetDepthTarget(depth, RenderGraphDepthAccess::ReadOnly, RenderGraphLoadOp::Load);
        },
        [this, commands = std::move(*commands)](ID3D11DeviceContext& deviceContext)
        {
            if (!ExecuteCommands(&deviceContext, commands))
            {
                throw std::runtime_error("Failed to execute debug line commands");
            }
        });
    return true;
}

// Expand each segment once; camera and vertex uploads remain deferred until execution.
std::optional<DebugLinePassCommands> DebugLinePass::PrepareCommands(
    const RenderFrameData& frame, const RenderView& view) const
{
    const auto viewConstants = PrepareViewConstants(view);
    if (!viewConstants) return std::nullopt;
    if (frame.debugLines.size() > MaxVertexCount / 2)
    {
        GILGAMESH_LOG(Renderer, Error, "Debug line vertices exceed the D3D11 buffer size limit");
        return std::nullopt;
    }

    DebugLinePassCommands commands;
    commands.view = *viewConstants;
    commands.vertices.reserve(frame.debugLines.size() * 2);
    for (const DebugLine& line : frame.debugLines)
    {
        if (!IsFinite(line.start) || !IsFinite(line.end) || !IsFinite(line.color))
        {
            GILGAMESH_LOG(Renderer, Error, "Cannot prepare nonfinite debug line positions or colors");
            return std::nullopt;
        }
        commands.vertices.push_back({ line.start.x, line.start.y, line.start.z,
            line.color.x, line.color.y, line.color.z });
        commands.vertices.push_back({ line.end.x, line.end.y, line.end.z,
            line.color.x, line.color.y, line.color.z });
    }
    return commands;
}

// Reuse capacity across frames, but never draw uninitialized vertices beyond this submission.
bool DebugLinePass::ExecuteCommands(
    ID3D11DeviceContext* context, const DebugLinePassCommands& commands) const
{
    if (!context || !IsInitialized())
    {
        GILGAMESH_LOG(Renderer, Error, "Cannot execute an uninitialized debug line pass or null context");
        return false;
    }
    if (commands.vertices.empty()) return true;
    if (commands.vertices.size() > MaxVertexCount || commands.vertices.size() % 2 != 0)
    {
        GILGAMESH_LOG(Renderer, Error, "Debug line commands require complete vertex pairs within D3D11 limits");
        return false;
    }

    // Upload only this view's data when its graph callback actually executes.
    const auto vertexCount = static_cast<std::uint32_t>(commands.vertices.size());
    if (!EnsureVertexCapacity(vertexCount) ||
        !vertexBuffer_.Update(context, commands.vertices.data(), vertexCount) ||
        !viewCB_.Update(context, commands.view))
    {
        GILGAMESH_LOG(Renderer, Error, "Failed to upload debug line commands");
        return false;
    }

    drawSetup_.Apply(context);
    vertexBuffer_.Bind(context);
    ID3D11Buffer* viewBuffer = viewCB_.GetBuffer();
    context->VSSetConstantBuffers(0, 1, &viewBuffer);
    context->Draw(vertexCount, 0);
    return true;
}

// Grow geometrically without exceeding the byte width accepted by VertexBuffer::Create.
bool DebugLinePass::EnsureVertexCapacity(std::uint32_t requiredVertexCount) const
{
    if (vertexBuffer_.GetBuffer() && vertexBuffer_.GetCount() >= requiredVertexCount) return true;

    const std::uint64_t grownCapacity =
        std::max<std::uint64_t>(256, static_cast<std::uint64_t>(vertexBuffer_.GetCount()) * 2);
    const auto capacity = static_cast<std::uint32_t>(std::min<std::uint64_t>(
        MaxVertexCount, std::max<std::uint64_t>(requiredVertexCount, grownCapacity)));

    // Dynamic buffers need no initial data; copying a smaller input array would overread it.
    VertexBuffer replacement;
    const HRESULT result = replacement.Create(device_.Get(),
        capacity * static_cast<std::uint32_t>(sizeof(SimpleVertex3D)),
        capacity, sizeof(SimpleVertex3D), nullptr, BUFFER_DYNAMIC);
    if (FAILED(result))
    {
        GILGAMESH_LOG(Renderer, Error, "Failed to grow debug line vertex storage");
        return false;
    }
    vertexBuffer_ = std::move(replacement);
    return true;
}

// Empty frames do not require an allocated dynamic vertex buffer.
bool DebugLinePass::IsInitialized() const noexcept
{
    return device_.Get() != nullptr && drawSetup_.IsInitialized() && viewCB_.IsInitialized();
}
