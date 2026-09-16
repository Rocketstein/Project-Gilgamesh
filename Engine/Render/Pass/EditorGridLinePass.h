#pragma once

#include "IRenderPass.h"

class EditorGridLinePass final : IRenderPass
{
public:

    void Execute(
        ID3D11DeviceContext* context,
        EditorViewport& target,
        const SimpleMeshRenderer& meshRenderer,
        const EditorViewportPassData& data) const;

private:
    EditorGridRenderer gridRenderer_;
};