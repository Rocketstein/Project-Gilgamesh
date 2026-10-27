#include <algorithm>
#include <cmath>
#include <limits>

#include "Engine/Core/BoundingShape.h"
#include "Tests/TestFramework.h"

namespace
{
// Enumerate eight corners independently of the interval projection used by TransformBounds.
BoundingBox ReferenceCornerBounds(const BoundingBox& bounds, const Matrix4& transform)
{
    double minimum[3]{
        std::numeric_limits<double>::infinity(),
        std::numeric_limits<double>::infinity(),
        std::numeric_limits<double>::infinity()
    };
    double maximum[3]{ -minimum[0], -minimum[1], -minimum[2] };
    for (int corner = 0; corner < 8; ++corner)
    {
        const float point[3]{
            (corner & 1) ? bounds.max.x : bounds.min.x,
            (corner & 2) ? bounds.max.y : bounds.min.y,
            (corner & 4) ? bounds.max.z : bounds.min.z
        };
        for (int axis = 0; axis < 3; ++axis)
        {
            double value = transform.m[3][axis];
            for (int sourceAxis = 0; sourceAxis < 3; ++sourceAxis)
            {
                value += static_cast<double>(point[sourceAxis]) * transform.m[sourceAxis][axis];
            }
            minimum[axis] = std::min(minimum[axis], value);
            maximum[axis] = std::max(maximum[axis], value);
        }
    }
    return {
        { static_cast<float>(minimum[0]), static_cast<float>(minimum[1]), static_cast<float>(minimum[2]) },
        { static_cast<float>(maximum[0]), static_cast<float>(maximum[1]), static_cast<float>(maximum[2]) }
    };
}
} // Anonymous namespace

GILGAMESH_TEST("Core.Math.BoundingShape", "ValidityCenterAndExtents")
{
    const BoundingBox bounds{ { -2.0f, 4.0f, 6.0f }, { 6.0f, 10.0f, 14.0f } };
    GILGAMESH_CHECK_MESSAGE(bounds.IsValid() && bounds.GetCenter() == Vector3(2.0f, 7.0f, 10.0f) &&
        bounds.GetExtents() == Vector3(4.0f, 3.0f, 4.0f),
        "ordered finite corners must produce their midpoint and half-extents");

    // Point and flat boxes remain useful bounds; reversed or nonfinite corners do not.
    const BoundingBox flat{ { 1.0f, 2.0f, 3.0f }, { 1.0f, 5.0f, 3.0f } };
    const BoundingBox reversed{ { 2.0f, 0.0f, 0.0f }, { 1.0f, 1.0f, 1.0f } };
    BoundingBox notFinite = bounds;
    notFinite.max.z = std::numeric_limits<float>::quiet_NaN();
    GILGAMESH_CHECK_MESSAGE(BoundingBox{}.IsValid() && flat.IsValid() &&
        !reversed.IsValid() && !notFinite.IsValid(),
        "zero extents must remain valid while reversed and nonfinite corners are rejected");
}

GILGAMESH_TEST("Core.Math.BoundingShape", "AffineBoundsMatchEightTransformedCorners")
{
    const BoundingBox bounds{ { -3.0f, 1.0f, -2.0f }, { 2.0f, 4.0f, 5.0f } };
    const Matrix4 transform{
        -1.5f, 0.75f, 0.2f, 0.0f,
        0.4f, 2.0f, -0.3f, 0.0f,
        -0.6f, 0.5f, 1.25f, 0.0f,
        7.0f, -4.0f, 2.0f, 1.0f
    };

    // The reference visits all corners of an off-center box under reflection and shear.
    const BoundingBox result = TransformBounds(bounds, transform);
    const BoundingBox expected = ReferenceCornerBounds(bounds, transform);
    GILGAMESH_CHECK_MESSAGE(result.IsValid() && NearlyEquals(result.min, expected.min) &&
        NearlyEquals(result.max, expected.max),
        "affine projection must enclose the same extrema as all eight transformed corners");
}

GILGAMESH_TEST("Core.Math.BoundingShape", "ZeroScaleProducesValidDegenerateBounds")
{
    const BoundingBox bounds{ { -1.0f, -2.0f, -3.0f }, { 2.0f, 3.0f, 4.0f } };
    const Matrix4 flatten{
        Matrix{ 0.0f, 0.0f, 0.0f, 0.0f, 2.0f, 0.0f, 0.0f, 0.0f, -3.0f },
        { 4.0f, 5.0f, 6.0f }
    };
    const BoundingBox flat = TransformBounds(bounds, flatten);
    GILGAMESH_CHECK_MESSAGE(flat.IsValid() && flat.min == Vector3(4.0f, 1.0f, -6.0f) &&
        flat.max == Vector3(4.0f, 11.0f, 15.0f),
        "negative and zero scales must produce ordered finite bounds without requiring invertibility");

    // Complete collapse is a valid point, not an invalid-input sentinel.
    const Matrix4 collapse{
        Matrix{ 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f },
        { 4.0f, 5.0f, 6.0f }
    };
    const BoundingBox point = TransformBounds(bounds, collapse);
    GILGAMESH_CHECK_MESSAGE(point.IsValid() && point.min == Vector3(4.0f, 5.0f, 6.0f) &&
        point.max == point.min,
        "collapsing every axis must still return a valid translated point bound");
}

GILGAMESH_TEST("Core.Math.BoundingShape", "RejectsInvalidSourceAndNonAffineMatrices")
{
    const BoundingBox bounds{ { -1.0f, -2.0f, -3.0f }, { 2.0f, 3.0f, 4.0f } };
    const BoundingBox reversed{ bounds.max, bounds.min };
    BoundingBox infiniteSource = bounds;
    infiniteSource.min.x = -std::numeric_limits<float>::infinity();
    Matrix4 notFinite = Matrix4::Identity();
    notFinite.m[0][0] = std::numeric_limits<float>::quiet_NaN();
    Matrix4 infiniteTranslation = Matrix4::Identity();
    infiniteTranslation.m[3][1] = std::numeric_limits<float>::infinity();
    Matrix4 perspective = Matrix4::Identity();
    perspective.m[1][3] = 0.25f;
    Matrix4 invalidHomogeneous = Matrix4::Identity();
    invalidHomogeneous.m[3][3] = 2.0f;

    // Every failure returns reversed corners rather than the valid default origin point.
    GILGAMESH_CHECK_MESSAGE(!TransformBounds(reversed, Matrix4::Identity()).IsValid() &&
        !TransformBounds(infiniteSource, Matrix4::Identity()).IsValid() &&
        !TransformBounds(bounds, notFinite).IsValid() &&
        !TransformBounds(bounds, infiniteTranslation).IsValid() &&
        !TransformBounds(bounds, perspective).IsValid() &&
        !TransformBounds(bounds, invalidHomogeneous).IsValid() && BoundingBox{}.IsValid(),
        "invalid sources, nonfinite matrix values, and perspective transforms must never produce valid bounds");
}

GILGAMESH_TEST("Core.Math.BoundingShape", "ExtremeFiniteIdentityPreservesOriginalCorners")
{
    constexpr float Limit = std::numeric_limits<float>::max();
    const BoundingBox symmetric{ { -Limit, -Limit, -Limit }, { Limit, Limit, Limit } };
    const BoundingBox positive{ { Limit * 0.5f, Limit * 0.5f, Limit * 0.5f }, { Limit, Limit, Limit } };
    GILGAMESH_CHECK_MESSAGE(symmetric.GetCenter() == Vector3{} &&
        symmetric.GetExtents() == Vector3(Limit, Limit, Limit) &&
        std::isfinite(positive.GetCenter().x) && std::isfinite(positive.GetExtents().x),
        "finite midpoint and half-extents must not overflow from intermediate sums or differences");

    // Asymmetric scales of magnitude must not lose a tiny signed endpoint through center cancellation.
    const float tiny = std::numeric_limits<float>::denorm_min();
    const BoundingBox asymmetric{ { -tiny, tiny, -Limit }, { Limit, Limit, Limit } };
    const BoundingBox symmetricResult = TransformBounds(symmetric, Matrix4::Identity());
    const BoundingBox asymmetricResult = TransformBounds(asymmetric, Matrix4::Identity());
    GILGAMESH_CHECK_MESSAGE(symmetricResult.IsValid() && symmetricResult.min == symmetric.min &&
        symmetricResult.max == symmetric.max && asymmetricResult.IsValid() &&
        asymmetricResult.min == asymmetric.min && asymmetricResult.max == asymmetric.max,
        "identity must preserve finite source corners across both extreme and asymmetric ranges");
}

GILGAMESH_TEST("Core.Math.BoundingShape", "UsesWideIntermediatesAndRejectsUnrepresentableOutput")
{
    constexpr float Limit = std::numeric_limits<float>::max();
    const BoundingBox bounds{ { Limit * 0.5f, 0.0f, 0.0f }, { Limit, 0.0f, 0.0f } };
    const Matrix scale{ 2.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f };

    // The product exceeds float range, but translation brings the final interval back into range.
    const BoundingBox canceled = TransformBounds(bounds, Matrix4{ scale, { -Limit, 0.0f, 0.0f } });
    GILGAMESH_CHECK_MESSAGE(canceled.IsValid() && canceled.min == Vector3{} &&
        canceled.max == Vector3(Limit, 0.0f, 0.0f),
        "representable final bounds must survive otherwise overflowing intermediate float products");
    GILGAMESH_CHECK_MESSAGE(!TransformBounds(bounds, Matrix4{ scale }).IsValid(),
        "a final interval exceeding float range must return invalid bounds instead of clamping or infinity");
}

GILGAMESH_TEST("Core.Math.BoundingShape", "NarrowingRoundsEnclosingEndpointsOutward")
{
    const BoundingBox point{ { 1.0f, 0.0f, 0.0f }, { 1.0f, 0.0f, 0.0f } };
    const Matrix4 transform{
        Matrix{ 0.1f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f },
        { 0.2f, 0.0f, 0.0f }
    };

    // The exact double product/sum lies between float values and must remain inside the result.
    const double expectedX = static_cast<double>(transform.m[0][0]) + transform.m[3][0];
    const BoundingBox result = TransformBounds(point, transform);
    GILGAMESH_CHECK_MESSAGE(result.IsValid() && static_cast<double>(result.min.x) <= expectedX &&
        static_cast<double>(result.max.x) >= expectedX &&
        result.min.y == 0.0f && result.max.y == 0.0f,
        "rounding back to float must not exclude a transformed point from its enclosing interval");
}
