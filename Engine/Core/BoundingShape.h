#pragma once

#include "Engine/Core/Math/Matrix.h"
#include "Engine/Core/Math/Vector.h"

// Axis-aligned bounds reusable in any coordinate space.
struct BoundingBox
{
public:
    // Requires finite, ordered corners; zero extents remain valid.
    [[nodiscard]] bool IsValid() const noexcept;

    // Returns the midpoint of the corners; callers must supply valid bounds.
    [[nodiscard]] Vector3 GetCenter() const noexcept;

    // Returns the half-size along each axis; callers must supply valid bounds.
    [[nodiscard]] Vector3 GetExtents() const noexcept;

public:
    Vector3 min{}; // Minimum corner.
    Vector3 max{}; // Maximum corner.
};

// Encloses all corners after an affine transform, including shear and negative scale.
[[nodiscard]] BoundingBox TransformBounds(
    const BoundingBox& bounds, const Matrix4& localToWorld) noexcept;
