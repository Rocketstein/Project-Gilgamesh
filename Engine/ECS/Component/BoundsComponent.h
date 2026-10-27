#pragma once

#include <cstdint>

#include "Engine/Core/BoundingShape.h"
#include "Engine/Render/Mesh/MeshHandle.h"

// Caches world-space render bounds for rendering and editor queries.
// The bounds updater is the sole writer and never clears transform change state.
struct AABBComponent
{
    BoundingBox AABB{}; // Evaluated world-space bounds, meaningful only when valid.
    bool valid = false; // Unevaluated or unavailable source bounds remain invalid.
    std::uint64_t revision = 0; // Publication token after first evaluation, even invalid; changes with the result.
    std::uint64_t sourceWorldRevision = 0; // World-transform revision used for the last evaluation.
    MeshHandle sourceMesh{}; // Mesh identity used for the last evaluation.
    std::uint64_t sourceMeshBoundsRevision = 0; // Mesh-local bounds revision used for the last evaluation.
};
