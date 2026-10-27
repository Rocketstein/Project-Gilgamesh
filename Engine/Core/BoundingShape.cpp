#include "BoundingShape.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace
{
// A reversed box cannot be confused with a valid point-sized bound at the origin.
BoundingBox InvalidBounds() noexcept
{
    return { { 1.0f, 1.0f, 1.0f }, { -1.0f, -1.0f, -1.0f } };
}
} // Anonymous namespace

// Degenerate boxes remain valid when both corners are finite and ordered.
bool BoundingBox::IsValid() const noexcept
{
    return std::isfinite(min.x) && std::isfinite(min.y) && std::isfinite(min.z) &&
        std::isfinite(max.x) && std::isfinite(max.y) && std::isfinite(max.z) &&
        min.x <= max.x && min.y <= max.y && min.z <= max.z;
}

// Double intermediates keep a finite midpoint even when the sum exceeds float range.
Vector3 BoundingBox::GetCenter() const noexcept
{
    return {
        static_cast<float>((static_cast<double>(min.x) + max.x) * 0.5),
        static_cast<float>((static_cast<double>(min.y) + max.y) * 0.5),
        static_cast<float>((static_cast<double>(min.z) + max.z) * 0.5)
    };
}

// Subtraction occurs in double so opposite finite extremes still produce finite half-extents.
Vector3 BoundingBox::GetExtents() const noexcept
{
    return {
        static_cast<float>((static_cast<double>(max.x) - min.x) * 0.5),
        static_cast<float>((static_cast<double>(max.y) - min.y) * 0.5),
        static_cast<float>((static_cast<double>(max.z) - min.z) * 0.5)
    };
}

// Project each source interval onto the world axes without rounding its center or extents first.
BoundingBox TransformBounds(
    const BoundingBox& bounds, const Matrix4& localToWorld) noexcept
{
    // Bounds evaluation accepts affine geometry only; perspective requires homogeneous division.
    if (!bounds.IsValid()) return InvalidBounds();
    for (const auto& row : localToWorld.m)
    {
        for (float value : row)
        {
            if (!std::isfinite(value)) return InvalidBounds();
        }
    }
    if (localToWorld.m[0][3] != 0.0f || localToWorld.m[1][3] != 0.0f ||
        localToWorld.m[2][3] != 0.0f || localToWorld.m[3][3] != 1.0f)
    {
        return InvalidBounds();
    }

    // Wider interval products avoid intermediate float overflow and preserve asymmetric bounds.
    const float sourceMin[3]{ bounds.min.x, bounds.min.y, bounds.min.z };
    const float sourceMax[3]{ bounds.max.x, bounds.max.y, bounds.max.z };
    float resultMin[3]{};
    float resultMax[3]{};
    constexpr double FloatLimit = std::numeric_limits<float>::max();
    constexpr float Infinity = std::numeric_limits<float>::infinity();
    for (int axis = 0; axis < 3; ++axis)
    {
        double lower = localToWorld.m[3][axis];
        double upper = lower;
        for (int sourceAxis = 0; sourceAxis < 3; ++sourceAxis)
        {
            const double coefficient = localToWorld.m[sourceAxis][axis];
            const double first = coefficient * sourceMin[sourceAxis];
            const double second = coefficient * sourceMax[sourceAxis];
            lower += std::min(first, second);
            upper += std::max(first, second);
        }

        // An enclosing finite float box cannot represent an out-of-range result.
        if (!std::isfinite(lower) || !std::isfinite(upper) ||
            lower < -FloatLimit || upper > FloatLimit)
        {
            return InvalidBounds();
        }

        // Round outward so narrowing to float cannot shrink the computed enclosing interval.
        resultMin[axis] = static_cast<float>(lower);
        resultMax[axis] = static_cast<float>(upper);
        if (static_cast<double>(resultMin[axis]) > lower)
        {
            resultMin[axis] = std::nextafter(resultMin[axis], -Infinity);
        }
        if (static_cast<double>(resultMax[axis]) < upper)
        {
            resultMax[axis] = std::nextafter(resultMax[axis], Infinity);
        }
    }

    // Keep the public result contract uniform after the final narrowing conversions.
    const BoundingBox result{
        { resultMin[0], resultMin[1], resultMin[2] },
        { resultMax[0], resultMax[1], resultMax[2] }
    };
    return result.IsValid() ? result : InvalidBounds();
}
