#pragma once

#include "Matrix.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>

// Quaternion stored as an imaginary vector (x, y, z) and a real scalar (w).
// Unit quaternions represent 3-D rotations without the singularities introduced
// by composing Euler angles.

// The convention is: Euler for human-facing rotation in/out, Quat for internal math.

struct Quat
{
	float x = 0.0f;
	float y = 0.0f;
	float z = 0.0f;
	float w = 1.0f;

	constexpr Quat() = default;
	constexpr Quat(float inX, float inY, float inZ, float inW) noexcept
		: x(inX), y(inY), z(inZ), w(inW) {}
	constexpr Quat(const Vector3& imaginary, float real) noexcept
		: x(imaginary.x), y(imaginary.y), z(imaginary.z), w(real) {}

	[[nodiscard]] static constexpr Quat Identity() noexcept { return {}; }

	// Contiguous XYZW for graphics APIs that take a float[4].
	[[nodiscard]] constexpr const float* Data() const noexcept { return &x; }
	[[nodiscard]] constexpr float* Data() noexcept { return &x; }
	[[nodiscard]] constexpr Vector3 Imaginary() const noexcept { return { x, y, z }; }

	constexpr Quat& operator+=(const Quat& rhs) noexcept
	{
		x += rhs.x; y += rhs.y; z += rhs.z; w += rhs.w;
		return *this;
	}

	constexpr Quat& operator-=(const Quat& rhs) noexcept
	{
		x -= rhs.x; y -= rhs.y; z -= rhs.z; w -= rhs.w;
		return *this;
	}

	constexpr Quat& operator*=(float scalar) noexcept
	{
		x *= scalar; y *= scalar; z *= scalar; w *= scalar;
		return *this;
	}

	constexpr Quat& operator/=(float scalar) noexcept
	{
		const float inverse = 1.0f / scalar;
		return *this *= inverse;
	}

	// Composition follows the engine's row-vector convention: lhs *= rhs means
	// "apply lhs, then rhs". This is the reverse of the written Hamilton product.
	constexpr Quat& operator*=(const Quat& rhs) noexcept
	{
		const Quat lhs = *this;
		const Quat next = rhs; // Keep q *= q safe while overwriting our components.
		x = next.w * lhs.x + next.x * lhs.w + next.y * lhs.z - next.z * lhs.y;
		y = next.w * lhs.y - next.x * lhs.z + next.y * lhs.w + next.z * lhs.x;
		z = next.w * lhs.z + next.x * lhs.y - next.y * lhs.x + next.z * lhs.w;
		w = next.w * lhs.w - next.x * lhs.x - next.y * lhs.y - next.z * lhs.z;
		return *this;
	}

	// Exact component comparison. A quaternion and its negation are different
	// values even though SameRotation reports that they encode the same rotation.
	[[nodiscard]] constexpr bool operator==(const Quat&) const = default;
};

static_assert(sizeof(Quat) == 4 * sizeof(float),
	"Quat must be tightly packed to alias as float[4].");

[[nodiscard]] constexpr Quat operator-(const Quat& q) noexcept { return { -q.x, -q.y, -q.z, -q.w }; }
[[nodiscard]] constexpr Quat operator+(const Quat& q) noexcept { return q; }

[[nodiscard]] constexpr Quat operator+(Quat lhs, const Quat& rhs) noexcept { return lhs += rhs; }
[[nodiscard]] constexpr Quat operator-(Quat lhs, const Quat& rhs) noexcept { return lhs -= rhs; }
[[nodiscard]] constexpr Quat operator*(Quat q, float scalar) noexcept { return q *= scalar; }
[[nodiscard]] constexpr Quat operator*(float scalar, Quat q) noexcept { return q *= scalar; }
[[nodiscard]] constexpr Quat operator/(Quat q, float scalar) noexcept { return q /= scalar; }

// Like matrix composition, lhs * rhs applies lhs first and rhs second.
[[nodiscard]] constexpr Quat operator*(Quat lhs, const Quat& rhs) noexcept { return lhs *= rhs; }

[[nodiscard]] constexpr float Dot(const Quat& a, const Quat& b) noexcept
{
	return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
}

[[nodiscard]] constexpr float LengthSquared(const Quat& q) noexcept { return Dot(q, q); }

namespace QuatDetail
{
	[[nodiscard]] constexpr bool IsFinite(float value) noexcept
	{
		return value == value
			&& value <= std::numeric_limits<float>::max()
			&& value >= -std::numeric_limits<float>::max();
	}

	[[nodiscard]] constexpr bool IsFinite(const Quat& q) noexcept
	{
		return IsFinite(q.x) && IsFinite(q.y) && IsFinite(q.z) && IsFinite(q.w);
	}

	[[nodiscard]] constexpr float Absolute(float value) noexcept
	{
		return value < 0.0f ? -value : value;
	}

	// Dividing by the largest component before taking a squared length keeps
	// normalization away from both overflow and underflow.
	[[nodiscard]] constexpr float MaxAbsComponent(const Quat& q) noexcept
	{
		float result = Absolute(q.x);
		if (Absolute(q.y) > result) result = Absolute(q.y);
		if (Absolute(q.z) > result) result = Absolute(q.z);
		if (Absolute(q.w) > result) result = Absolute(q.w);
		return result;
	}

	[[nodiscard]] constexpr float MaxAbsComponent(const Vector3& v) noexcept
	{
		float result = Absolute(v.x);
		if (Absolute(v.y) > result) result = Absolute(v.y);
		if (Absolute(v.z) > result) result = Absolute(v.z);
		return result;
	}

	[[nodiscard]] constexpr bool IsUsableRotation(const Quat& q) noexcept
	{
		return IsFinite(q) && MaxAbsComponent(q) > 0.0f;
	}
}

// Rescaling first lets Length return a finite answer whenever the true norm is
// representable, even if the sum of squared components is not.
[[nodiscard]] inline float Length(const Quat& q) noexcept
{
	if (!QuatDetail::IsFinite(q))
		return std::sqrt(LengthSquared(q));

	const float largestComponent = QuatDetail::MaxAbsComponent(q);
	if (largestComponent == 0.0f)
		return 0.0f;

	const Quat scaled{
		q.x / largestComponent,
		q.y / largestComponent,
		q.z / largestComponent,
		q.w / largestComponent
	};
	return largestComponent * std::sqrt(LengthSquared(scaled));
}

// A degenerate quaternion cannot describe an orientation, so identity is the
// safe neutral rotation. This also prevents NaNs from reaching transform code.
[[nodiscard]] inline Quat Normalized(const Quat& q) noexcept
{
	if (!QuatDetail::IsFinite(q))
		return Quat::Identity();

	const float largestComponent = QuatDetail::MaxAbsComponent(q);
	if (largestComponent == 0.0f)
		return Quat::Identity();

	// Divide components directly: forming 1 / largestComponent first would
	// overflow when the quaternion contains only subnormal values.
	const Quat scaled{
		q.x / largestComponent,
		q.y / largestComponent,
		q.z / largestComponent,
		q.w / largestComponent
	};
	const Quat result = scaled * (1.0f / std::sqrt(LengthSquared(scaled)));
	return QuatDetail::IsFinite(result) ? result : Quat::Identity();
}

[[nodiscard]] constexpr Quat Conjugate(const Quat& q) noexcept
{
	return { -q.x, -q.y, -q.z, q.w };
}

// False leaves out untouched. Unlike normalization, inversion is algebraic and
// therefore accepts every finite, non-zero quaternion whose inverse remains
// representable as floats.
[[nodiscard]] constexpr bool TryInverse(const Quat& q, Quat& out) noexcept
{
	if (!QuatDetail::IsFinite(q))
		return false;

	const float largestComponent = QuatDetail::MaxAbsComponent(q);
	if (largestComponent == 0.0f)
		return false;

	const Quat scaled{
		q.x / largestComponent,
		q.y / largestComponent,
		q.z / largestComponent,
		q.w / largestComponent
	};
	const Quat scaledInverse = Conjugate(scaled) / LengthSquared(scaled);
	const Quat candidate{
		scaledInverse.x / largestComponent,
		scaledInverse.y / largestComponent,
		scaledInverse.z / largestComponent,
		scaledInverse.w / largestComponent
	};
	if (!QuatDetail::IsFinite(candidate))
		return false;

	out = candidate;
	return true;
}

[[nodiscard]] constexpr std::optional<Quat> Inverse(const Quat& q) noexcept
{
	Quat result;
	if (!TryInverse(q, result))
		return std::nullopt;
	return result;
}

[[nodiscard]] inline bool NearlyEquals(
	const Quat& a,
	const Quat& b,
	float epsilon = 1e-5f) noexcept
{
	if (epsilon < 0.0f || !QuatDetail::IsFinite(epsilon))
		return false;

	return std::fabs(a.x - b.x) <= epsilon
		&& std::fabs(a.y - b.y) <= epsilon
		&& std::fabs(a.z - b.z) <= epsilon
		&& std::fabs(a.w - b.w) <= epsilon;
}

// Rotations have a double cover: q and -q describe the same orientation.
[[nodiscard]] inline bool SameRotation(
	const Quat& a,
	const Quat& b,
	float epsilon = 1e-5f) noexcept
{
	if (epsilon < 0.0f
		|| !QuatDetail::IsFinite(epsilon)
		|| !QuatDetail::IsUsableRotation(a)
		|| !QuatDetail::IsUsableRotation(b))
	{
		return false;
	}

	const Quat normalizedA = Normalized(a);
	const Quat normalizedB = Normalized(b);
	return NearlyEquals(normalizedA, normalizedB, epsilon)
		|| NearlyEquals(normalizedA, -normalizedB, epsilon);
}

// The axis may have any finite, non-zero length. Invalid inputs fall back to
// identity, matching the neutral-rotation policy used by Normalized and Rotate.
[[nodiscard]] inline Quat FromAxisAngle(
	const Vector3& axis,
	float angleRadians) noexcept
{
	if (!QuatDetail::IsFinite(axis.x)
		|| !QuatDetail::IsFinite(axis.y)
		|| !QuatDetail::IsFinite(axis.z)
		|| !QuatDetail::IsFinite(angleRadians))
	{
		return Quat::Identity();
	}

	const float largestComponent = QuatDetail::MaxAbsComponent(axis);
	// Keep the divisor valid when a zero-axis call is constant-folded.
	const float divisor = largestComponent == 0.0f ? 1.0f : largestComponent;
	if (largestComponent == 0.0f)
		return Quat::Identity();

	const Vector3 scaledAxis{
		axis.x / divisor,
		axis.y / divisor,
		axis.z / divisor
	};
	const Vector3 unitAxis = scaledAxis * (1.0f / std::sqrt(LengthSquared(scaledAxis)));
	const float halfAngle = angleRadians * 0.5f;
	return Normalized({ unitAxis * std::sin(halfAngle), std::cos(halfAngle) });
}

// Expanding the Hamilton product q (v, 0) conjugate(q) avoids constructing two
// temporary quaternions. Normalizing here makes scaled representations safe.
[[nodiscard]] inline Vector3 Rotate(const Vector3& vector, const Quat& rotation) noexcept
{
	const Quat unitRotation = Normalized(rotation);
	const Vector3 imaginary = unitRotation.Imaginary();
	const Vector3 twiceCross = 2.0f * Cross(imaginary, vector);
	return vector + unitRotation.w * twiceCross + Cross(imaginary, twiceCross);
}

// Row vectors multiply rotations on the right, just as they do with Matrix.
[[nodiscard]] inline Vector3 operator*(const Vector3& vector, const Quat& rotation) noexcept
{
	return Rotate(vector, rotation);
}

// Produces the row-vector rotation matrix used throughout Matrix.h.
[[nodiscard]] inline Matrix ToMatrix(const Quat& rotation) noexcept
{
	const Quat q = Normalized(rotation);
	const float xx = q.x * q.x;
	const float yy = q.y * q.y;
	const float zz = q.z * q.z;
	const float xy = q.x * q.y;
	const float xz = q.x * q.z;
	const float yz = q.y * q.z;
	const float xw = q.x * q.w;
	const float yw = q.y * q.w;
	const float zw = q.z * q.w;

	return {
		1.0f - 2.0f * (yy + zz), 2.0f * (xy + zw),        2.0f * (xz - yw),
		2.0f * (xy - zw),        1.0f - 2.0f * (xx + zz), 2.0f * (yz + xw),
		2.0f * (xz + yw),        2.0f * (yz - xw),        1.0f - 2.0f * (xx + yy)
	};
}

[[nodiscard]] inline Matrix4 ToMatrix4(const Quat& rotation) noexcept
{
	return Matrix4{ ToMatrix(rotation) };
}

// The input is expected to be an orthonormal row-vector rotation matrix. The
// largest-diagonal branches retain precision near 180-degree rotations.
[[nodiscard]] inline Quat FromRotationMatrix(const Matrix& matrix) noexcept
{
	for (int row = 0; row < 3; ++row)
		for (int column = 0; column < 3; ++column)
			if (!QuatDetail::IsFinite(matrix.m[row][column]))
				return Quat::Identity();

	Quat result;
	const float trace = matrix.m[0][0] + matrix.m[1][1] + matrix.m[2][2];
	float root = 0.0f;

	if (trace > 0.0f)
	{
		root = std::sqrt(trace + 1.0f);
		if (!(root > 0.0f))
			return Quat::Identity();

		const float scale = 0.5f / root;
		result = {
			(matrix.m[1][2] - matrix.m[2][1]) * scale,
			(matrix.m[2][0] - matrix.m[0][2]) * scale,
			(matrix.m[0][1] - matrix.m[1][0]) * scale,
			0.5f * root
		};
	}
	else if (matrix.m[0][0] >= matrix.m[1][1]
		&& matrix.m[0][0] >= matrix.m[2][2])
	{
		root = std::sqrt(std::max(
			0.0f,
			1.0f + matrix.m[0][0] - matrix.m[1][1] - matrix.m[2][2]));
		if (!(root > 0.0f))
			return Quat::Identity();

		const float scale = 0.5f / root;
		result = {
			0.5f * root,
			(matrix.m[0][1] + matrix.m[1][0]) * scale,
			(matrix.m[0][2] + matrix.m[2][0]) * scale,
			(matrix.m[1][2] - matrix.m[2][1]) * scale
		};
	}
	else if (matrix.m[1][1] >= matrix.m[2][2])
	{
		root = std::sqrt(std::max(
			0.0f,
			1.0f + matrix.m[1][1] - matrix.m[0][0] - matrix.m[2][2]));
		if (!(root > 0.0f))
			return Quat::Identity();

		const float scale = 0.5f / root;
		result = {
			(matrix.m[0][1] + matrix.m[1][0]) * scale,
			0.5f * root,
			(matrix.m[1][2] + matrix.m[2][1]) * scale,
			(matrix.m[2][0] - matrix.m[0][2]) * scale
		};
	}
	else
	{
		root = std::sqrt(std::max(
			0.0f,
			1.0f + matrix.m[2][2] - matrix.m[0][0] - matrix.m[1][1]));
		if (!(root > 0.0f))
			return Quat::Identity();

		const float scale = 0.5f / root;
		result = {
			(matrix.m[0][2] + matrix.m[2][0]) * scale,
			(matrix.m[1][2] + matrix.m[2][1]) * scale,
			0.5f * root,
			(matrix.m[0][1] - matrix.m[1][0]) * scale
		};
	}

	return Normalized(result);
}

[[nodiscard]] inline Quat FromRotationMatrix(const Matrix4& matrix) noexcept
{
	return FromRotationMatrix(matrix.Basis());
}

// Uses the shorter arc by flipping one endpoint when the dot product is
// negative. Amount is clamped to the endpoint range, and nearly parallel
// rotations use normalized lerp to avoid a tiny denominator.
[[nodiscard]] inline Quat Slerp(const Quat& from, const Quat& to, float amount) noexcept
{
	if (!QuatDetail::IsFinite(amount))
		return Quat::Identity();

	const float interpolationAmount = std::clamp(amount, 0.0f, 1.0f);
	const Quat start = Normalized(from);
	Quat finish = Normalized(to);
	float cosine = Dot(start, finish);

	if (cosine < 0.0f)
	{
		finish = -finish;
		cosine = -cosine;
	}

	cosine = std::clamp(cosine, -1.0f, 1.0f);
	if (cosine > 0.9995f)
		return Normalized(start + (finish - start) * interpolationAmount);

	const float angle = std::acos(cosine);
	const float sine = std::sin(angle);
	if (std::fabs(sine) <= 1e-6f)
		return Normalized(start + (finish - start) * interpolationAmount);

	const float startWeight = std::sin((1.0f - interpolationAmount) * angle) / sine;
	const float finishWeight = std::sin(interpolationAmount * angle) / sine;
	return Normalized(start * startWeight + finish * finishWeight);
}
