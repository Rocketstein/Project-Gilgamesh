#pragma once

struct RenderPassContext
{
    ID3D11DeviceContext& deviceContext;
    const RenderView& view;
    Extent2D targetExtent;
};