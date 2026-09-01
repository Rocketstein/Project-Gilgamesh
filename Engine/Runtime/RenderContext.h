#pragma once

#include "Engine/Render/Renderer/Renderer.h"

struct RenderContext
{
	Renderer& renderer;
	Extent2D outputExtent;
};