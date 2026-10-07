#pragma once

#include <cstdint>

#include "Engine/Render/DrawSetup/DrawSetup.h"

class VertexBuffer;
class IndexBuffer;

struct RenderCommand
{
	// Sorting key
	uint64_t sortKey = 0;

	// Geometry
	DrawSetup* setup;

	// Draw Arguments
	uint32_t indexCount = 0;
	uint32_t startIndex = 0;
	uint64_t instanceCount = 0;
};