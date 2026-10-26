#include "BoundingShape.h"

bool BoundingBox::IsValid() const noexcept
{
    // Placeholder until finite-corner and ordering validation is implemented.
    return false;
}

Vector3 BoundingBox::GetCenter() const noexcept
{
    // Placeholder until bounds-center evaluation is implemented.
    return {};
}

Vector3 BoundingBox::GetExtents() const noexcept
{
    // Placeholder until half-extents evaluation is implemented.
    return {};
}

BoundingBox TransformBounds(
    const BoundingBox& /*bounds*/, const Matrix4& /*localToWorld*/) noexcept
{
    // Placeholder until affine corner enclosure is implemented.
    return {};
}
