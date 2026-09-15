#pragma once

#include <cstdint>
#include <vector>

#include "Engine/Render/Types/Vertex.h"

// Should implement MeshAsset/SubMesh later on
struct StaticMeshData
{

	std::vector<NormalVertex> vertices_;
	std::vector<std::uint32_t> indices_;
};
