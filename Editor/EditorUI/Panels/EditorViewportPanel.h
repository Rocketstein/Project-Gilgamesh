#pragma once

#include "Engine/Core/Extent2D.h"

class EditorViewport;

struct EditorViewportPanelFrame
{
    bool shouldRender = false;
    bool imageHovered = false;
    bool focused = false;
    Extent2D renderExtent{};
};

class EditorViewportPanel
{
public:
    inline static constexpr char WindowName[] =
        "Editor Viewport";

    [[nodiscard]]
    EditorViewportPanelFrame Draw(
        EditorViewport& viewport);
};
