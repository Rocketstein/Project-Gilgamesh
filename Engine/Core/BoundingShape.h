#pragma once

#include "Engine/Core/Math/Matrix.h"
#include "Engine/Core/Math/Vector.h"

// Axis-aligned bounds reusable in any coordinate space.
struct BoundingBox
{
public:
    // Requires finite, ordered corners; zero extents remain valid.
    [[nodiscard]] bool IsValid() const noexcept;

    // Returns the midpoint using wider intermediate arithmetic; callers must supply valid bounds.
    [[nodiscard]] Vector3 GetCenter() const noexcept;

    // Returns the half-size without intermediate overflow; callers must supply valid bounds.
    [[nodiscard]] Vector3 GetExtents() const noexcept;

public:
    Vector3 min{}; // Minimum corner.
    Vector3 max{}; // Maximum corner.
};

// Encloses all corners after a finite affine transform, including shear and negative/zero scale.
// Invalid input or an unrepresentable result returns a box with reversed corners, never a point box.
[[nodiscard]] BoundingBox TransformBounds(
    const BoundingBox& bounds, const Matrix4& localToWorld) noexcept;
