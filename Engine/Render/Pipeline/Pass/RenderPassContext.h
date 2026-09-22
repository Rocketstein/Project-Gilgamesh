#pragma once

#include <d3d11.h>

#include "Engine/Core/Extent2D.h"
#include "Engine/Render/Renderer/RenderView.h"

// Shared per-view state; the caller binds the target and viewport before Execute.
struct RenderPassContext
{
    ID3D11DeviceContext& deviceContext;
    const RenderView& view;
    Extent2D targetExtent;
};
