#include <array>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <limits>
#include <optional>
#include <string_view>
#include <thread>
#include <utility>

#include <Windows.h>
#include <d3d11.h>
#include <wrl/client.h>

#include "Editor/EditorViewport/EditorViewport.h"
#include "Engine/Render/Buffers/VertexBuffer.h"
#include "Engine/Render/Mesh/MeshManager.h"
#include "Engine/Render/Passes/DebugLinePass.h"
#include "Engine/Render/RenderGraph/RenderGraph.h"
#include "Engine/Render/Shader/ShaderManager.h"
#include "Tests/TestFramework.h"

namespace
{
// Share software rendering resources while each submission starts from cleared immediate state.
struct TestDevice
{
    Microsoft::WRL::ComPtr<ID3D11Device> device;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
};

// WARP makes shader, buffer, and readback checks independent of physical graphics hardware.
const TestDevice& GetTestDevice()
{
    static const TestDevice device = []
    {
        TestDevice result;
        const HRESULT created = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP,
            nullptr, 0, nullptr, 0, D3D11_SDK_VERSION, result.device.GetAddressOf(),
            nullptr, result.context.GetAddressOf());
        if (FAILED(created))
        {
            result.device.Reset();
            result.context.Reset();
        }
        return result;
    }();
    return device;
}

// Tests receive separately compiled shader assets beside their own executable.
std::filesystem::path GetShaderDirectory()
{
    std::array<wchar_t, MAX_PATH> executablePath{};
    const DWORD length = GetModuleFileNameW(nullptr, executablePath.data(),
        static_cast<DWORD>(executablePath.size()));
    if (length == 0 || length == executablePath.size()) return {};
    return std::filesystem::path{
        std::wstring_view{ executablePath.data(), length }
    }.parent_path() / L"Shaders";
}

// Initialize the real pass and attachments while reporting unavailable test infrastructure as failure.
bool InitializePass(Gilgamesh::Testing::TestContext& testContext,
    DebugLinePass& pass, ShaderManager& shaders, EditorViewport& viewport)
{
    const TestDevice& device = GetTestDevice();
    const bool initialized = device.device && device.context &&
        shaders.Initialize(device.device.Get(), GetShaderDirectory()) &&
        pass.Initialize(device.device.Get(), shaders);
    GILGAMESH_CHECK_MESSAGE(initialized,
        "the WARP device, debug line shaders, and pass must initialize");
    if (!initialized) return false;
    device.context->ClearState();
    viewport.Initialize(device.device.Get());
    viewport.RequestResize({ 64, 64 });
    const bool ready = viewport.ApplyPendingResize() && viewport.IsReady();
    GILGAMESH_CHECK_MESSAGE(ready, "the test viewport must provide usable color and depth views");
    return ready;
}

// Place a horizontal world-space segment exactly through the center pixel of a 64-pixel view.
DebugLine CenteredLine(Vector3 color, float depth = 0.2f, float centerX = 0.0f)
{
    return {
        { centerX - 0.75f, -1.0f / 64.0f, depth },
        { centerX + 0.75f, -1.0f / 64.0f, depth },
        color
    };
}

// Initialize imported resources outside the graph; the line pass is allowed only read-only depth.
void ClearViewport(const EditorViewport& viewport, float depth = 1.0f)
{
    const TestDevice& device = GetTestDevice();
    device.context->ClearState();
    constexpr float Black[4]{ 0.0f, 0.0f, 0.0f, 1.0f };
    device.context->ClearRenderTargetView(viewport.GetColorRenderTargetView(), Black);
    device.context->ClearDepthStencilView(viewport.GetDepthStencilView(), D3D11_CLEAR_DEPTH, depth, 0);
}

// Omit the writable DSV so compilation verifies the pass's declared depth access mode.
bool QueueLines(const DebugLinePass& pass, RenderGraph& graph,
    const EditorViewport& viewport, const RenderFrameData& frame, const RenderView& view)
{
    const auto color = graph.ImportTexture("DebugColor", {
        .extent = viewport.GetExtent(), .format = DXGI_FORMAT_R8G8B8A8_UNORM
    }, {
        .texture = viewport.GetColorTexture(),
        .renderTargetView = viewport.GetColorRenderTargetView()
    });
    const auto depth = graph.ImportTexture("DebugDepth", {
        .extent = viewport.GetExtent(), .format = DXGI_FORMAT_D32_FLOAT
    }, {
        .texture = viewport.GetDepthTexture(),
        .readOnlyDepthStencilView = viewport.GetReadOnlyDepthStencilView()
    });
    const MeshManager unusedMeshes;
    return pass.AddToGraph(graph, { color, depth, frame, view, unusedMeshes });
}

// Read RGBA8 or D32_FLOAT data after copying the viewport into CPU-readable staging storage.
template<typename Pixel>
std::optional<Pixel> ReadCenterPixel(ID3D11Texture2D* texture)
{
    static_assert(sizeof(Pixel) == 4);
    const TestDevice& device = GetTestDevice();
    D3D11_TEXTURE2D_DESC description{};
    texture->GetDesc(&description);
    description.Usage = D3D11_USAGE_STAGING;
    description.BindFlags = 0;
    description.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    description.MiscFlags = 0;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> staging;
    if (FAILED(device.device->CreateTexture2D(&description, nullptr, staging.GetAddressOf())))
        return std::nullopt;
    device.context->CopyResource(staging.Get(), texture);
    D3D11_MAPPED_SUBRESOURCE mapped{};
    if (FAILED(device.context->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &mapped))) return std::nullopt;
    const auto* source = static_cast<const std::uint8_t*>(mapped.pData) +
        (description.Height / 2) * mapped.RowPitch + (description.Width / 2) * sizeof(Pixel);
    Pixel result{};
    std::memcpy(&result, source, sizeof(Pixel));
    device.context->Unmap(staging.Get(), 0);
    return result;
}

// Count actual input-assembler vertices to distinguish active draw size from retained buffer capacity.
bool ExecuteAndCount(RenderGraph& graph, D3D11_QUERY_DATA_PIPELINE_STATISTICS& statistics)
{
    const TestDevice& device = GetTestDevice();
    if (!graph.Compile(device.device.Get())) return false;
    const D3D11_QUERY_DESC description{ D3D11_QUERY_PIPELINE_STATISTICS, 0 };
    Microsoft::WRL::ComPtr<ID3D11Query> query;
    if (FAILED(device.device->CreateQuery(&description, query.GetAddressOf()))) return false;
    device.context->Begin(query.Get());
    const bool executed = graph.Execute(device.context.Get());
    device.context->End(query.Get());
    if (!executed) return false;
    device.context->Flush();

    // Bound asynchronous query waiting so a device failure cannot stall the test executable.
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    HRESULT status = S_FALSE;
    while (status == S_FALSE && std::chrono::steady_clock::now() < deadline)
    {
        status = device.context->GetData(query.Get(), &statistics, sizeof(statistics), 0);
        if (status == S_FALSE) std::this_thread::yield();
    }
    return status == S_OK;
}

// Copy the initialized prefix of a GPU buffer without inspecting discarded, unused capacity.
template<typename Element, std::size_t Count>
bool ReadBufferPrefix(ID3D11Buffer* source, std::array<Element, Count>& result)
{
    if (!source) return false;
    const TestDevice& device = GetTestDevice();
    D3D11_BUFFER_DESC description{};
    source->GetDesc(&description);
    if (description.ByteWidth < sizeof(result)) return false;
    description.Usage = D3D11_USAGE_STAGING;
    description.BindFlags = 0;
    description.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    description.MiscFlags = 0;
    description.StructureByteStride = 0;
    Microsoft::WRL::ComPtr<ID3D11Buffer> staging;
    if (FAILED(device.device->CreateBuffer(&description, nullptr, staging.GetAddressOf()))) return false;
    device.context->CopyResource(staging.Get(), source);
    D3D11_MAPPED_SUBRESOURCE mapped{};
    if (FAILED(device.context->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &mapped))) return false;
    std::memcpy(result.data(), mapped.pData, sizeof(result));
    device.context->Unmap(staging.Get(), 0);
    return true;
}
} // Anonymous namespace

GILGAMESH_TEST("Engine.Render.DebugLinePass", "PreparesOwnedWorldVerticesAndValidatesInput")
{
    DebugLinePass pass;
    RenderFrameData frame;
    frame.debugLines.push_back({ { 1.0f, 2.0f, 3.0f }, { 4.0f, 5.0f, 6.0f }, { 0.0f, 1.0f, 0.5f } });
    RenderView view{ .extent = { 64, 64 } };
    view.view(3, 0) = -2.0f;
    const auto commands = pass.PrepareCommands(frame, view);
    frame.debugLines.clear();
    GILGAMESH_CHECK_MESSAGE(commands && commands->vertices.size() == 2 && !pass.IsInitialized(),
        "CPU preparation must own two vertices per segment without requiring GPU initialization");
    if (!commands || commands->vertices.size() != 2) return;
    const SimpleVertex3D& first = commands->vertices[0];
    const SimpleVertex3D& second = commands->vertices[1];
    GILGAMESH_CHECK_MESSAGE(first.x == 1.0f && first.y == 2.0f && first.z == 3.0f &&
        second.x == 4.0f && second.y == 5.0f && second.z == 6.0f &&
        first.r == 0.0f && first.g == 1.0f && first.b == 0.5f && second.g == first.g &&
        NearlyEquals(commands->view.viewProjection, Transpose(view.view * view.projection)),
        "commands must retain world-space vertices separately from the view-projection upload");

    // Reject invalid views and nonfinite segment data before recording any draw work.
    GILGAMESH_CHECK_MESSAGE(!pass.PrepareCommands(frame, RenderView{}),
        "zero-sized views must not produce commands");
    RenderView singular{ .extent = { 64, 64 } };
    singular.projection(0, 0) = 0.0f;
    GILGAMESH_CHECK_MESSAGE(!pass.PrepareCommands(frame, singular),
        "singular view-projection data must not produce commands");
    frame.debugLines.push_back(CenteredLine({ 0.0f, 1.0f, 0.0f }));
    frame.debugLines[0].start.x = std::numeric_limits<float>::quiet_NaN();
    GILGAMESH_CHECK_MESSAGE(!pass.PrepareCommands(frame, view),
        "nonfinite endpoints must be rejected before GPU upload");
    frame.debugLines[0] = CenteredLine({ 0.0f, std::numeric_limits<float>::infinity(), 0.0f });
    GILGAMESH_CHECK_MESSAGE(!pass.PrepareCommands(frame, view),
        "nonfinite RGB values must be rejected before GPU upload");
}

GILGAMESH_TEST("Engine.Render.DebugLinePass", "DepthTestsActualPixelsWithoutWritingSceneDepth")
{
    DebugLinePass pass;
    ShaderManager shaders;
    EditorViewport viewport;
    if (!InitializePass(testContext, pass, shaders, viewport)) return;
    ClearViewport(viewport, 0.4f);
    RenderFrameData frame;
    frame.debugLines = {
        CenteredLine({ 0.0f, 1.0f, 0.0f }, 0.2f),
        CenteredLine({ 1.0f, 0.0f, 0.0f }, 0.7f)
    };
    RenderGraph graph;
    const RenderView view{ .extent = { 64, 64 } };
    const bool executed = QueueLines(pass, graph, viewport, frame, view) &&
        graph.Compile(GetTestDevice().device.Get()) && graph.Execute(GetTestDevice().context.Get());
    const auto color = ReadCenterPixel<std::array<std::uint8_t, 4>>(viewport.GetColorTexture());
    const auto depth = ReadCenterPixel<float>(viewport.GetDepthTexture());
    const std::array<std::uint8_t, 4> green{ 0, 255, 0, 255 };
    GILGAMESH_CHECK_MESSAGE(executed && color && *color == green && depth && *depth == 0.4f,
        "a visible green line must draw, the red line behind scene depth must fail, and scene depth must remain unchanged");

    // Inspect the state applied by the pass as well as the read-only attachment behavior.
    D3D11_PRIMITIVE_TOPOLOGY topology{};
    GetTestDevice().context->IAGetPrimitiveTopology(&topology);
    Microsoft::WRL::ComPtr<ID3D11DepthStencilState> state;
    GetTestDevice().context->OMGetDepthStencilState(state.GetAddressOf(), nullptr);
    D3D11_DEPTH_STENCIL_DESC description{};
    if (state) state->GetDesc(&description);
    GILGAMESH_CHECK_MESSAGE(topology == D3D11_PRIMITIVE_TOPOLOGY_LINELIST && state &&
        description.DepthEnable && description.DepthWriteMask == D3D11_DEPTH_WRITE_MASK_ZERO &&
        description.DepthFunc == D3D11_COMPARISON_LESS_EQUAL,
        "the debug pass must use line lists and depth-tested non-writing state");
    GetTestDevice().context->ClearState();
}

GILGAMESH_TEST("Engine.Render.DebugLinePass", "GrowsStorageAndDrawsOnlyActiveVerticesIncludingEmptyFrames")
{
    DebugLinePass pass;
    ShaderManager shaders;
    EditorViewport viewport;
    if (!InitializePass(testContext, pass, shaders, viewport)) return;
    const TestDevice& device = GetTestDevice();
    const RenderView view{ .extent = { 64, 64 } };
    RenderFrameData frame;
    frame.debugLines.push_back(CenteredLine({ 0.0f, 1.0f, 0.0f }));
    ClearViewport(viewport);
    RenderGraph firstGraph;
    D3D11_QUERY_DATA_PIPELINE_STATISTICS firstStatistics{};
    GILGAMESH_CHECK_MESSAGE(QueueLines(pass, firstGraph, viewport, frame, view) &&
        ExecuteAndCount(firstGraph, firstStatistics) && firstStatistics.IAVertices == 2,
        "a single segment must submit exactly two active vertices");
    Microsoft::WRL::ComPtr<ID3D11Buffer> firstBuffer;
    UINT stride = 0;
    UINT offset = 0;
    device.context->IAGetVertexBuffers(0, 1, firstBuffer.GetAddressOf(), &stride, &offset);
    GILGAMESH_CHECK_MESSAGE(firstBuffer && stride == sizeof(SimpleVertex3D),
        "execution must bind compact position/RGB storage");
    if (!firstBuffer) return;
    D3D11_BUFFER_DESC firstDescription{};
    firstBuffer->GetDesc(&firstDescription);

    // Exceed the actual retained capacity instead of depending on a particular growth policy.
    const std::size_t largeLineCount = firstDescription.ByteWidth / sizeof(SimpleVertex3D) / 2 + 1;
    frame.debugLines.assign(largeLineCount, CenteredLine({ 0.0f, 0.0f, 1.0f }));
    ClearViewport(viewport);
    RenderGraph largeGraph;
    D3D11_QUERY_DATA_PIPELINE_STATISTICS largeStatistics{};
    GILGAMESH_CHECK_MESSAGE(QueueLines(pass, largeGraph, viewport, frame, view) &&
        ExecuteAndCount(largeGraph, largeStatistics) && largeStatistics.IAVertices == largeLineCount * 2,
        "growing the dynamic buffer must submit the full larger batch exactly once");
    Microsoft::WRL::ComPtr<ID3D11Buffer> grownBuffer;
    device.context->IAGetVertexBuffers(0, 1, grownBuffer.GetAddressOf(), &stride, &offset);
    D3D11_BUFFER_DESC grownDescription{};
    if (grownBuffer) grownBuffer->GetDesc(&grownDescription);
    GILGAMESH_CHECK_MESSAGE(grownBuffer && grownDescription.ByteWidth > firstDescription.ByteWidth,
        "a batch beyond capacity must allocate sufficient replacement storage");

    // Smaller and empty frames must never draw stale capacity from a preceding larger batch.
    frame.debugLines.assign(1, CenteredLine({ 1.0f, 0.0f, 0.0f }));
    ClearViewport(viewport);
    RenderGraph smallGraph;
    D3D11_QUERY_DATA_PIPELINE_STATISTICS smallStatistics{};
    GILGAMESH_CHECK_MESSAGE(QueueLines(pass, smallGraph, viewport, frame, view) &&
        ExecuteAndCount(smallGraph, smallStatistics) && smallStatistics.IAVertices == 2,
        "retained buffer capacity must not become the smaller frame's draw count");
    frame.debugLines.clear();
    ClearViewport(viewport);
    RenderGraph emptyGraph;
    D3D11_QUERY_DATA_PIPELINE_STATISTICS emptyStatistics{};
    const bool emptyExecuted = QueueLines(pass, emptyGraph, viewport, frame, view) &&
        ExecuteAndCount(emptyGraph, emptyStatistics);
    const auto pixel = ReadCenterPixel<std::array<std::uint8_t, 4>>(viewport.GetColorTexture());
    const std::array<std::uint8_t, 4> black{ 0, 0, 0, 255 };
    GILGAMESH_CHECK_MESSAGE(emptyExecuted && emptyStatistics.IAVertices == 0 && pixel && *pixel == black,
        "an empty frame must submit no stale lines and preserve the preexisting clear color");
    device.context->ClearState();
}

GILGAMESH_TEST("Engine.Render.DebugLinePass", "KeepsQueuedFrameAndViewSnapshotsIndependent")
{
    DebugLinePass pass;
    ShaderManager shaders;
    EditorViewport firstViewport;
    if (!InitializePass(testContext, pass, shaders, firstViewport)) return;
    const TestDevice& device = GetTestDevice();
    EditorViewport secondViewport;
    secondViewport.Initialize(device.device.Get());
    secondViewport.RequestResize({ 64, 64 });
    GILGAMESH_CHECK_MESSAGE(secondViewport.ApplyPendingResize() && secondViewport.IsReady(),
        "the second queued view must have independent attachments");
    if (!secondViewport.IsReady()) return;
    ClearViewport(firstViewport);
    ClearViewport(secondViewport);
    RenderFrameData frame;
    frame.debugLines.push_back(CenteredLine({ 0.0f, 1.0f, 0.0f }, 0.2f, 2.0f));
    RenderView firstView{ .extent = { 64, 64 } };
    firstView.view(3, 0) = -2.0f;
    RenderGraph firstGraph;
    GILGAMESH_CHECK_MESSAGE(QueueLines(pass, firstGraph, firstViewport, frame, firstView),
        "the first world-space line and translated view must prepare successfully");

    // Queue a different frame and camera before either graph executes.
    frame.debugLines[0] = CenteredLine({ 1.0f, 0.0f, 0.0f }, 0.2f, -2.0f);
    RenderView secondView{ .extent = { 64, 64 } };
    secondView.view(3, 0) = 2.0f;
    RenderGraph secondGraph;
    GILGAMESH_CHECK_MESSAGE(QueueLines(pass, secondGraph, secondViewport, frame, secondView),
        "the second frame and view must prepare independently");
    frame.debugLines.clear();
    firstView.view(3, 0) = 100.0f;
    secondView.view(3, 0) = -100.0f;
    Microsoft::WRL::ComPtr<ID3D11Buffer> beforeExecution;
    device.context->IAGetVertexBuffers(0, 1, beforeExecution.GetAddressOf(), nullptr, nullptr);
    GILGAMESH_CHECK_MESSAGE(!beforeExecution,
        "preparing queued views must not allocate, upload, or bind the shared vertex buffer");

    // Reverse execution order exposes premature GPU uploads and references to caller-owned data.
    const bool executed = secondGraph.Compile(device.device.Get()) &&
        firstGraph.Compile(device.device.Get()) && secondGraph.Execute(device.context.Get()) &&
        firstGraph.Execute(device.context.Get());
    const auto firstPixel = ReadCenterPixel<std::array<std::uint8_t, 4>>(firstViewport.GetColorTexture());
    const auto secondPixel = ReadCenterPixel<std::array<std::uint8_t, 4>>(secondViewport.GetColorTexture());
    const std::array<std::uint8_t, 4> green{ 0, 255, 0, 255 };
    const std::array<std::uint8_t, 4> red{ 255, 0, 0, 255 };
    GILGAMESH_CHECK_MESSAGE(executed && firstPixel && *firstPixel == green &&
        secondPixel && *secondPixel == red,
        "each queued graph must draw its owned lines using its own camera at execution time");
    device.context->ClearState();
}

GILGAMESH_TEST("Engine.Render.DebugLinePass", "RejectsUnusableSubmissionsWithoutPublishingPartialState")
{
    const TestDevice& device = GetTestDevice();
    ShaderManager shaders;
    DebugLinePass pass;
    GILGAMESH_CHECK_MESSAGE(!pass.Initialize(nullptr, shaders) && !pass.IsInitialized(),
        "failed initialization must leave the default pass uninitialized");
    EditorViewport viewport;
    if (!InitializePass(testContext, pass, shaders, viewport)) return;
    GILGAMESH_CHECK_MESSAGE(!pass.Initialize(nullptr, shaders) && pass.IsInitialized(),
        "failed reinitialization must preserve existing usable pass resources");
    RenderFrameData frame;
    frame.debugLines.push_back(CenteredLine({ 0.0f, 1.0f, 0.0f }));
    const RenderView view{ .extent = { 64, 64 } };
    const auto commands = pass.PrepareCommands(frame, view);
    GILGAMESH_CHECK_MESSAGE(commands && !pass.ExecuteCommands(nullptr, *commands),
        "execution must reject a null context");
    DebugLinePassCommands incomplete;
    incomplete.vertices.push_back({});
    GILGAMESH_CHECK_MESSAGE(!pass.ExecuteCommands(device.context.Get(), incomplete),
        "execution must reject an incomplete line pair");
    RenderGraph graph;
    MeshManager meshes;
    GILGAMESH_CHECK_MESSAGE(!pass.AddToGraph(graph, { {}, {}, frame, view, meshes }) &&
        graph.GetPassCount() == 0,
        "invalid graph targets must not append a partial pass declaration");
    device.context->ClearState();
}

GILGAMESH_TEST("Engine.Render.VertexBuffer", "UploadsDynamicDataWithoutInitialAllocationContents")
{
    const TestDevice& device = GetTestDevice();
    GILGAMESH_CHECK_MESSAGE(device.device && device.context, "a WARP device must be available");
    if (!device.device || !device.context) return;
    device.context->ClearState();
    VertexBuffer buffer;
    constexpr std::uint32_t Capacity = 4;
    const std::array<std::uint32_t, 2> uploaded{ 17, 29 };
    GILGAMESH_CHECK_MESSAGE(SUCCEEDED(buffer.Create(device.device.Get(), Capacity * sizeof(std::uint32_t),
        Capacity, sizeof(std::uint32_t), nullptr, BUFFER_DYNAMIC)) &&
        buffer.Update(device.context.Get(), uploaded.data(), static_cast<std::uint32_t>(uploaded.size())),
        "dynamic buffers must allow uninitialized allocation followed by a bounded upload");
    std::array<std::uint32_t, 2> readback{};
    GILGAMESH_CHECK_MESSAGE(buffer.GetCount() == Capacity && ReadBufferPrefix(buffer.GetBuffer(), readback) &&
        readback == uploaded,
        "readback must expose uploaded data while GetCount retains the allocation capacity");
}

GILGAMESH_TEST("Engine.Render.VertexBuffer", "RejectsInvalidUploadsAndPreservesPublishedBuffers")
{
    const TestDevice& device = GetTestDevice();
    GILGAMESH_CHECK_MESSAGE(device.device && device.context, "a WARP device must be available");
    if (!device.device || !device.context) return;
    device.context->ClearState();
    const std::array<std::uint32_t, 5> values{ 1, 2, 3, 4, 5 };
    VertexBuffer buffer;
    GILGAMESH_CHECK_MESSAGE(SUCCEEDED(buffer.Create(device.device.Get(), 4 * sizeof(std::uint32_t),
        4, sizeof(std::uint32_t), nullptr, BUFFER_DYNAMIC)) &&
        buffer.Update(device.context.Get(), values.data(), 2), "the initial dynamic upload must succeed");
    GILGAMESH_CHECK_MESSAGE(!buffer.Update(nullptr, values.data(), 2) &&
        !buffer.Update(device.context.Get(), nullptr, 2) &&
        !buffer.Update(device.context.Get(), values.data(), 0) &&
        !buffer.Update(device.context.Get(), values.data(), 5),
        "invalid pointers, empty uploads, and writes beyond capacity must be reported as failure");
    std::array<std::uint32_t, 2> readback{};
    GILGAMESH_CHECK_MESSAGE(ReadBufferPrefix(buffer.GetBuffer(), readback) && readback[0] == 1 && readback[1] == 2,
        "rejected uploads must preserve the previously published GPU contents");
    ID3D11Buffer* original = buffer.GetBuffer();
    GILGAMESH_CHECK_MESSAGE(FAILED(buffer.Create(device.device.Get(), 4 * sizeof(std::uint32_t),
        4, sizeof(std::uint32_t), nullptr, BUFFER_IMMUTABLE)) && buffer.GetBuffer() == original &&
        FAILED(buffer.Create(device.device.Get(), sizeof(std::uint32_t), 4, sizeof(std::uint32_t),
            values.data(), BUFFER_DYNAMIC)) && buffer.GetBuffer() == original,
        "invalid creation must preserve the old buffer and reject missing immutable data or undersized allocation");
    VertexBuffer immutable;
    GILGAMESH_CHECK_MESSAGE(SUCCEEDED(immutable.Create(device.device.Get(), 4 * sizeof(std::uint32_t),
        4, sizeof(std::uint32_t), values.data())) &&
        !immutable.Update(device.context.Get(), values.data(), 2),
        "immutable vertex buffers must reject dynamic uploads");
}
