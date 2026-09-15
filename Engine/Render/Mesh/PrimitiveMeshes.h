#pragma once

#include "StaticMeshData.h"

namespace PrimitiveMeshes
{
	// Builds the current unit cube with shared corners and diagnostic vertex colors.
	[[nodiscard]]
	StaticMeshData MakeUnitCubeMeshData();
}
