#include "EditorGridLinePass.h"

PassDescriptor EditorGridLinePass::Describe()
{
    return {
        .name = "EditorGrid",
        .vertexShader = L"EditorGrid",
        .pixelShader = L"EditorGrid",
        .drawState = {
            .topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST,
            .cullMode = D3D11_CULL_NONE,
            .fillMode = D3D11_FILL_SOLID,
            .depthTestEnabled = true,
            .depthWriteEnabled = false,
            .depthComparison = D3D11_COMPARISON_LESS_EQUAL,
            .blendEnabled = true
        }
    };
}

void EditorGridLinePass::Execute(RenderPassContext& context, const RenderFrameData& frame)
{
    // GPU resource preparation and drawing are added after registration.
}

