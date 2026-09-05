#include "Engine/Core/Math/Matrix.h"

#include <DirectXMath.h>

#include <cmath>
#include <cstdio>
#include <limits>
#include <random>
#include <string_view>

namespace
{
int failures = 0;

void Check(bool condition, std::string_view message)
{
    if (condition)
        return;

    std::fprintf(
        stderr,
        "FAILED: %.*s\n",
        static_cast<int>(message.size()),
        message.data());
    ++failures;
}

void TestInteropAndTransforms()
{
    const Matrix4 transform{
        2.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 3.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 4.0f, 0.0f,
        5.0f, 6.0f, 7.0f, 1.0f
    };

    Check(FromDirectX(ToDirectX(transform)) == transform,
        "DirectX conversion must preserve every component");
    Check(NearlyEquals(
        TransformPoint({ 1.0f, 2.0f, 3.0f }, transform),
        Vector3{ 7.0f, 12.0f, 19.0f }),
        "TransformPoint must apply scale and translation");
    Check(NearlyEquals(
        TransformDirection({ 1.0f, 2.0f, 3.0f }, transform),
        Vector3{ 2.0f, 6.0f, 12.0f }),
        "TransformDirection must ignore translation");

    const Vector4 homogeneous{ 1.0f, 2.0f, 3.0f, 1.0f };
    Check(NearlyEquals(
        homogeneous * transform,
        Vector4{ 7.0f, 12.0f, 19.0f, 1.0f }),
        "Vector4 multiplication must preserve the row-vector convention");
}

void TestCompositionAndTranspose()
{
    const Matrix4 translation{
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        3.0f, 4.0f, 5.0f, 1.0f
    };
    const Matrix4 scale{
        2.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 3.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 4.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f
    };

    const Vector3 point{ 1.0f, 1.0f, 1.0f };
    Check(NearlyEquals(
        TransformPoint(point, translation * scale),
        TransformPoint(TransformPoint(point, translation), scale)),
        "A * B must apply A before B");
    Check(Transpose(Transpose(translation)) == translation,
        "double transpose must recover the input");
    Check((2.0f * Matrix4::Identity()) - Matrix4::Identity()
            == Matrix4::Identity(),
        "scalar, addition, and subtraction operators must use every component");
    Check(std::fabs(Determinant(scale) - 24.0f) <= 1e-5f,
        "determinant must match the product of diagonal scale factors");
}

void TestInverseFailures()
{
    const Matrix4 sentinel{
        2.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 2.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 2.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 2.0f
    };
    const Matrix4 singular{
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f
    };

    Matrix4 output = sentinel;
    Check(!TryInverse(singular, output),
        "TryInverse must reject singular matrices");
    Check(output == sentinel,
        "TryInverse must leave output untouched on failure");
    Check(!Inverse(singular).has_value(),
        "Inverse must expose singularity instead of returning identity");

    Matrix4 notFinite = Matrix4::Identity();
    notFinite(0, 0) = std::numeric_limits<float>::quiet_NaN();
    output = sentinel;
    Check(!TryInverse(notFinite, output),
        "TryInverse must reject non-finite input");
    Check(output == sentinel,
        "non-finite failure must leave output untouched");
    Check(!NearlyEquals(notFinite, Matrix4::Identity()),
        "NearlyEquals must reject NaN components");
    Check(!NearlyEquals(Matrix4::Identity(), Matrix4::Identity(), -1.0f),
        "NearlyEquals must reject negative tolerances");
    Check(!NearlyEquals(
        Matrix4::Identity(),
        Matrix4::Identity(),
        std::numeric_limits<float>::infinity()),
        "NearlyEquals must reject non-finite tolerances");

    Matrix2 singular2{ 1.0f, 2.0f, 2.0f, 4.0f };
    Matrix2 output2{ 2.0f, 0.0f, 0.0f, 2.0f };
    const Matrix2 sentinel2 = output2;
    Check(!TryInverse(singular2, output2) && output2 == sentinel2,
        "Matrix2 inverse failure must be observable and non-mutating");
    Check(!Inverse(singular2).has_value(),
        "Matrix2 Inverse must expose singularity");
    Matrix2 notFinite2 = Matrix2::Identity();
    notFinite2(0, 0) = std::numeric_limits<float>::quiet_NaN();
    Check(!NearlyEquals(notFinite2, Matrix2::Identity()),
        "Matrix2 NearlyEquals must reject NaN components");

    Matrix singular3{
        1.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f
    };
    Matrix output3{
        2.0f, 0.0f, 0.0f,
        0.0f, 2.0f, 0.0f,
        0.0f, 0.0f, 2.0f
    };
    const Matrix sentinel3 = output3;
    Check(!TryInverse(singular3, output3) && output3 == sentinel3,
        "Matrix3 inverse failure must be observable and non-mutating");
    Check(!Inverse(singular3).has_value(),
        "Matrix3 Inverse must expose singularity");
    Matrix notFinite3 = Matrix::Identity();
    notFinite3(0, 0) = std::numeric_limits<float>::quiet_NaN();
    Check(!NearlyEquals(notFinite3, Matrix::Identity()),
        "Matrix3 NearlyEquals must reject NaN components");
}

void TestRandomInverses()
{
    std::mt19937 random(0x47494c47u);
    std::uniform_real_distribution<float> value(-4.0f, 4.0f);
    int tested = 0;

    for (int sample = 0; sample < 2000; ++sample)
    {
        const Matrix4 matrix{
            value(random), value(random), value(random), value(random),
            value(random), value(random), value(random), value(random),
            value(random), value(random), value(random), value(random),
            value(random), value(random), value(random), value(random)
        };

        const auto inverse = Inverse(matrix);
        if (!inverse)
            continue;

        Check(NearlyEquals(matrix * *inverse, Matrix4::Identity(), 5e-3f),
            "matrix multiplied by its inverse must be identity");
        ++tested;
    }

    Check(tested > 1900, "random inverse test must exercise enough matrices");
}
}

int main()
{
    TestInteropAndTransforms();
    TestCompositionAndTranspose();
    TestInverseFailures();
    TestRandomInverses();

    if (failures != 0)
        std::fprintf(stderr, "%d matrix test(s) failed\n", failures);

    return failures == 0 ? 0 : 1;
}
