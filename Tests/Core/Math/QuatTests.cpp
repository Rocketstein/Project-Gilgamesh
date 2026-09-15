#include "Engine/Core/Math/Quat.h"
#include "Tests/TestFramework.h"

#include <cmath>
#include <limits>
#include <numbers>
#include <random>

namespace
{
constexpr float Pi = std::numbers::pi_v<float>;

[[nodiscard]] bool NearlyEqual(float a, float b, float epsilon = 1e-5f) noexcept
{
    return std::fabs(a - b) <= epsilon;
}
}

GILGAMESH_TEST("Core.Math.Quat", "ValueOperations")
{
    const Quat identity;
    GILGAMESH_CHECK_MESSAGE(identity == Quat::Identity(),
        "default construction must produce the identity rotation");

    const Quat value{ 1.0f, 2.0f, 3.0f, 4.0f };
    GILGAMESH_CHECK_MESSAGE(value.Data()[0] == 1.0f && value.Data()[3] == 4.0f,
        "Data must expose contiguous XYZW components");
    GILGAMESH_CHECK_MESSAGE((value.Imaginary() == Vector3{ 1.0f, 2.0f, 3.0f }),
        "Imaginary must return the quaternion's vector part");
    GILGAMESH_CHECK_MESSAGE(
        ((value + Quat{ 4.0f, 3.0f, 2.0f, 1.0f })
            == Quat{ 5.0f, 5.0f, 5.0f, 5.0f }),
        "addition must use every component");
    GILGAMESH_CHECK_MESSAGE(
        (((value - Quat{ 1.0f, 1.0f, 1.0f, 1.0f }) * 0.5f)
            == Quat{ 0.0f, 0.5f, 1.0f, 1.5f }),
        "subtraction and scalar multiplication must use every component");
    GILGAMESH_CHECK_MESSAGE(NearlyEqual(Length(value), std::sqrt(30.0f)),
        "Length must include every component");
    GILGAMESH_CHECK_MESSAGE(
        Length(Quat{ 1e20f, 0.0f, 0.0f, 0.0f }) == 1e20f
            && Length(Quat{ 1e-30f, 0.0f, 0.0f, 0.0f }) == 1e-30f,
        "Length must preserve representable norms across float scales");
    GILGAMESH_CHECK_MESSAGE(NearlyEquals(
            Normalized(Quat{ 0.0f, 0.0f, 0.0f, 2.0f }), identity),
        "normalization must produce a unit quaternion");
    GILGAMESH_CHECK_MESSAGE(
        Normalized(Quat{ 0.0f, 0.0f, 0.0f, 0.0f }) == identity,
        "a degenerate rotation must normalize to identity");
}

GILGAMESH_TEST("Core.Math.Quat", "AxisAngleAndVectorRotation")
{
    const Quat quarterTurn = FromAxisAngle({ 0.0f, 0.0f, 4.0f }, Pi * 0.5f);
    const Quat rotateX = FromAxisAngle({ 1.0f, 0.0f, 0.0f }, Pi * 0.5f);
    const Quat rotateY = FromAxisAngle({ 0.0f, 1.0f, 0.0f }, Pi * 0.5f);
    const float halfSqrtTwo = std::sqrt(0.5f);
    const float smallestPositive = std::numeric_limits<float>::denorm_min();

    GILGAMESH_CHECK_MESSAGE(
        NearlyEquals(quarterTurn, { 0.0f, 0.0f, halfSqrtTwo, halfSqrtTwo }),
        "axis-angle construction must normalize its axis");
    GILGAMESH_CHECK_MESSAGE(
        NearlyEquals(Rotate({ 1.0f, 0.0f, 0.0f }, quarterTurn), { 0.0f, 1.0f, 0.0f }),
        "a positive Z rotation must turn +X toward +Y");
    GILGAMESH_CHECK_MESSAGE(
        NearlyEquals(Rotate({ 0.0f, 1.0f, 0.0f }, rotateX), { 0.0f, 0.0f, 1.0f }),
        "a positive X rotation must turn +Y toward +Z");
    GILGAMESH_CHECK_MESSAGE(
        NearlyEquals(Rotate({ 0.0f, 0.0f, 1.0f }, rotateY), { 1.0f, 0.0f, 0.0f }),
        "a positive Y rotation must turn +Z toward +X");
    GILGAMESH_CHECK_MESSAGE(
        NearlyEquals(
            Rotate({ 1.0f, 0.0f, 0.0f }, quarterTurn * 3.0f),
            { 0.0f, 1.0f, 0.0f }),
        "Rotate must accept a scaled representation of a valid quaternion");
    GILGAMESH_CHECK_MESSAGE(
        NearlyEquals(
            Rotate({ 1.0f, 0.0f, 0.0f }, quarterTurn * 1e20f),
            { 0.0f, 1.0f, 0.0f }),
        "rotation normalization must avoid overflow at large finite scales");
    GILGAMESH_CHECK_MESSAGE(
        NearlyEquals(
            Rotate({ 1.0f, 0.0f, 0.0f }, quarterTurn * 1e-20f),
            { 0.0f, 1.0f, 0.0f }),
        "rotation normalization must avoid underflow at small finite scales");
    GILGAMESH_CHECK_MESSAGE(
        SameRotation(
            FromAxisAngle({ 0.0f, 0.0f, 1e20f }, Pi * 0.5f), quarterTurn)
            && SameRotation(
                FromAxisAngle({ 0.0f, 0.0f, 1e-20f }, Pi * 0.5f), quarterTurn),
        "axis normalization must be independent of every finite non-zero scale");
    GILGAMESH_CHECK_MESSAGE(
        SameRotation(
            Normalized(Quat{ 0.0f, 0.0f, smallestPositive, smallestPositive }),
            quarterTurn)
            && SameRotation(
                FromAxisAngle({ 0.0f, 0.0f, smallestPositive }, Pi * 0.5f),
                quarterTurn),
        "normalization must preserve rotations represented by subnormal values");
    GILGAMESH_CHECK_MESSAGE(
        NearlyEquals(ToMatrix(quarterTurn), Matrix{
            0.0f, 1.0f, 0.0f,
            -1.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f }),
        "quaternion matrices must use the engine's row-vector orientation");
    GILGAMESH_CHECK_MESSAGE(FromAxisAngle({}, Pi) == Quat::Identity(),
        "a zero rotation axis must safely produce identity");
    GILGAMESH_CHECK_MESSAGE(
        FromAxisAngle({ 0.0f, 0.0f, 1.0f }, std::numeric_limits<float>::infinity())
            == Quat::Identity(),
        "a non-finite angle must safely produce identity");
}

GILGAMESH_TEST("Core.Math.Quat", "CompositionAndMatrices")
{
    const Quat rotateX = FromAxisAngle({ 1.0f, 0.0f, 0.0f }, Pi * 0.5f);
    const Quat rotateY = FromAxisAngle({ 0.0f, 1.0f, 0.0f }, Pi * 0.5f);
    const Quat composed = rotateX * rotateY;
    const Vector3 vector{ 0.0f, 1.0f, 0.0f };

    Quat assigned = rotateX;
    assigned *= rotateY;
    GILGAMESH_CHECK_MESSAGE(NearlyEquals(assigned, composed),
        "compound multiplication must use the same composition order");

    Quat selfComposed = rotateX;
    selfComposed *= selfComposed;
    GILGAMESH_CHECK_MESSAGE(SameRotation(selfComposed, rotateX * rotateX),
        "compound multiplication must be safe when both operands alias");

    GILGAMESH_CHECK_MESSAGE(
        NearlyEquals(vector * composed, (vector * rotateX) * rotateY),
        "quaternion products must apply rotations from left to right");
    GILGAMESH_CHECK_MESSAGE(
        NearlyEquals(Rotate(vector, composed), { 1.0f, 0.0f, 0.0f }),
        "non-commuting composition must preserve the documented order");
    GILGAMESH_CHECK_MESSAGE(
        NearlyEquals(ToMatrix(composed), ToMatrix(rotateX) * ToMatrix(rotateY)),
        "quaternion and row-vector matrix composition must agree");
    GILGAMESH_CHECK_MESSAGE(
        NearlyEquals(Rotate(vector, composed), vector * ToMatrix(composed)),
        "direct vector rotation and matrix rotation must agree");
    GILGAMESH_CHECK_MESSAGE(
        NearlyEquals(
            TransformDirection(vector, ToMatrix4(composed)),
            Rotate(vector, composed)),
        "3x3 and 4x4 quaternion matrices must encode the same rotation");

    const Quat roundTrip = FromRotationMatrix(ToMatrix(composed));
    GILGAMESH_CHECK_MESSAGE(SameRotation(roundTrip, composed),
        "a quaternion must survive a rotation-matrix round trip");
    GILGAMESH_CHECK_MESSAGE(
        SameRotation(FromRotationMatrix(ToMatrix4(composed)), composed),
        "the Matrix4 conversion must read only the rotation basis");

    const Quat branchRotations[] = {
        Quat::Identity(),
        FromAxisAngle({ 1.0f, 0.0f, 0.0f }, Pi),
        FromAxisAngle({ 0.0f, 1.0f, 0.0f }, Pi),
        FromAxisAngle({ 0.0f, 0.0f, 1.0f }, Pi)
    };
    for (const Quat& rotation : branchRotations)
    {
        GILGAMESH_CHECK_MESSAGE(
            SameRotation(FromRotationMatrix(ToMatrix(rotation)), rotation),
            "matrix conversion must handle identity and every dominant diagonal");
    }
}

GILGAMESH_TEST("Core.Math.Quat", "InverseAndFailureHandling")
{
    const Quat value{ 1.0f, -2.0f, 3.0f, 4.0f };
    const auto inverse = Inverse(value);
    GILGAMESH_CHECK_MESSAGE(inverse.has_value(),
        "every finite non-zero quaternion must be invertible when representable");
    GILGAMESH_CHECK_MESSAGE(
        inverse.has_value() && NearlyEquals(value * *inverse, Quat::Identity()),
        "a quaternion multiplied by its inverse must be identity");

    const Quat rotation = FromAxisAngle({ 1.0f, 2.0f, 3.0f }, 0.75f);
    const float scales[] = { 1e-20f, 1e20f };
    for (const float scale : scales)
    {
        const Quat scaled = rotation * scale;
        const auto scaledInverse = Inverse(scaled);
        GILGAMESH_CHECK_MESSAGE(
            scaledInverse.has_value()
                && NearlyEquals(scaled * *scaledInverse, Quat::Identity()),
            "inverse calculation must avoid squared-norm underflow and overflow");
    }

    const Quat subnormal{ 1e-39f, 1e-39f, 1e-39f, 1e-39f };
    const auto subnormalInverse = Inverse(subnormal);
    GILGAMESH_CHECK_MESSAGE(
        subnormalInverse.has_value()
            && NearlyEquals(subnormal * *subnormalInverse, Quat::Identity(), 1e-4f),
        "inverse calculation must preserve representable subnormal inputs");

    const Quat sentinel{ 5.0f, 6.0f, 7.0f, 8.0f };
    Quat output = sentinel;
    GILGAMESH_CHECK_MESSAGE(
        !TryInverse(Quat{ 0.0f, 0.0f, 0.0f, 0.0f }, output),
        "TryInverse must reject the zero quaternion");
    GILGAMESH_CHECK_MESSAGE(output == sentinel,
        "TryInverse must leave output untouched on failure");
    GILGAMESH_CHECK_MESSAGE(!Inverse(Quat{
            std::numeric_limits<float>::quiet_NaN(), 0.0f, 0.0f, 1.0f }).has_value(),
        "Inverse must expose non-finite input as failure");
}

GILGAMESH_TEST("Core.Math.Quat", "RotationEquivalenceAndSlerp")
{
    const Quat start = FromAxisAngle({ 0.0f, 0.0f, 1.0f }, 0.0f);
    const Quat finish = FromAxisAngle({ 0.0f, 0.0f, 1.0f }, Pi * 0.5f);

    GILGAMESH_CHECK_MESSAGE(!NearlyEquals(finish, -finish),
        "component comparison must distinguish a quaternion from its negation");
    GILGAMESH_CHECK_MESSAGE(SameRotation(finish, -finish),
        "rotation comparison must account for the quaternion double cover");
    GILGAMESH_CHECK_MESSAGE(SameRotation(Slerp(start, finish, 0.0f), start),
        "Slerp amount zero must return the starting rotation");
    GILGAMESH_CHECK_MESSAGE(SameRotation(Slerp(start, finish, 1.0f), finish),
        "Slerp amount one must return the finishing rotation");
    GILGAMESH_CHECK_MESSAGE(
        SameRotation(Slerp(start, finish, -1.0f), start)
            && SameRotation(Slerp(start, finish, 2.0f), finish),
        "Slerp must clamp interpolation amounts to its endpoint range");
    GILGAMESH_CHECK_MESSAGE(
        NearlyEquals(
            Rotate({ 1.0f, 0.0f, 0.0f }, Slerp(start, finish, 0.5f)),
            { std::sqrt(0.5f), std::sqrt(0.5f), 0.0f }),
        "Slerp midpoint must travel halfway along the rotation arc");
    GILGAMESH_CHECK_MESSAGE(SameRotation(Slerp(finish, -finish, 0.5f), finish),
        "Slerp must choose the short path between antipodal representations");
}

GILGAMESH_TEST("Core.Math.Quat", "RandomCompositionAndRoundTrips")
{
    std::mt19937 random{ 0x514154u };
    std::uniform_real_distribution<float> component{ -1.0f, 1.0f };

    for (int iteration = 0; iteration < 1000; ++iteration)
    {
        Quat a{ component(random), component(random), component(random), component(random) };
        Quat b{ component(random), component(random), component(random), component(random) };
        if (LengthSquared(a) <= 0.01f || LengthSquared(b) <= 0.01f)
        {
            --iteration;
            continue;
        }

        a = Normalized(a);
        b = Normalized(b);
        const Vector3 vector{ component(random), component(random), component(random) };
        const Quat composed = a * b;

        GILGAMESH_CHECK_MESSAGE(
            NearlyEquals(vector * composed, (vector * a) * b, 2e-5f),
            "random quaternion composition must match sequential rotation");
        GILGAMESH_CHECK_MESSAGE(
            NearlyEquals(ToMatrix(composed), ToMatrix(a) * ToMatrix(b), 2e-5f),
            "random quaternion composition must match matrix composition");
        GILGAMESH_CHECK_MESSAGE(
            SameRotation(FromRotationMatrix(ToMatrix(a)), a, 2e-5f),
            "random rotation matrices must round-trip to the same orientation");
    }
}
