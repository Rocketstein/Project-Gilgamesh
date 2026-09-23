#pragma once

#include <cstdint>

class VertexBuffer;
class IndexBuffer;

struct RenderCommand
{
	// Sorting key
	uint64_t sortKey = 0;

	// Geometry
	VertexBuffer* VB = nullptr;
	IndexBuffer* IB = nullptr;

	// Draw Arguments
	uint32_t indexCount = 0;
	uint32_t startIndex = 0;
	uint64_t instanceCount = 0;
};