#pragma once
#include <cmath>

// Storage-format vector types.

// A 2-D Vector
struct Vector2
{
	float x = 0.0f;
	float y = 0.0f;

	constexpr Vector2() = default;
	constexpr Vector2(float inX, float inY) noexcept : x(inX), y(inY) {}

	// Contiguous XY for graphics APIs that take a float[2].
	[[nodiscard]] constexpr const float* Data() const noexcept { return &x; }
	[[nodiscard]] constexpr float* Data() noexcept { return &x; }

	// Compound assignment carries the actual arithmetic; the binary operators
	// below are written in terms of these so there is one place to fix.
	constexpr Vector2& operator+=(const Vector2& rhs) noexcept { x += rhs.x; y += rhs.y; return *this; }
	constexpr Vector2& operator-=(const Vector2& rhs) noexcept { x -= rhs.x; y -= rhs.y; return *this; }
	constexpr Vector2& operator*=(float scalar) noexcept { x *= scalar; y *= scalar; return *this; }
	constexpr Vector2& operator/=(float scalar) noexcept
	{
		const float inv = 1.0f / scalar;	// One divide instead of two.
		x *= inv; y *= inv;
		return *this;
	}

	// Scales in place and returns *this so calls chain. The free Scale below
	// is the non-mutating form.
	constexpr Vector2& Scale(float scalar) noexcept { return *this *= scalar; }

	// Non-uniform scale: the component-wise product the operators deliberately
	// leave out.
	constexpr Vector2& Scale(const Vector2& factors) noexcept
	{
		x *= factors.x; y *= factors.y;
		return *this;
	}

	// Exact component comparison. Use NearlyEquals for anything downstream of
	// a floating-point computation.
	[[nodiscard]] constexpr bool operator==(const Vector2&) const = default;
};

static_assert(sizeof(Vector2) == 2 * sizeof(float),
	"Vector2 must be tightly packed to alias as float[2].");

[[nodiscard]] constexpr Vector2 operator-(const Vector2& v) noexcept { return { -v.x, -v.y }; }
[[nodiscard]] constexpr Vector2 operator+(const Vector2& v) noexcept { return v; }

// Taking lhs by value lets the compound operator do the work and gives the
// copy elision the canonical form is built around.
[[nodiscard]] constexpr Vector2 operator+(Vector2 lhs, const Vector2& rhs) noexcept { return lhs += rhs; }
[[nodiscard]] constexpr Vector2 operator-(Vector2 lhs, const Vector2& rhs) noexcept { return lhs -= rhs; }

// Scalar multiply is commutative, so it needs both orderings. The left-hand
// scalar form cannot be a member.
[[nodiscard]] constexpr Vector2 operator*(Vector2 v, float scalar) noexcept { return v *= scalar; }
[[nodiscard]] constexpr Vector2 operator*(float scalar, Vector2 v) noexcept { return v *= scalar; }
[[nodiscard]] constexpr Vector2 operator/(Vector2 v, float scalar) noexcept { return v /= scalar; }

// Non-mutating counterparts to the members. [[nodiscard]] so that writing
// Scale(v, 2.0f); as a statement, expecting mutation, is a warning.
[[nodiscard]] constexpr Vector2 Scale(Vector2 v, float scalar) noexcept { return v.Scale(scalar); }
[[nodiscard]] constexpr Vector2 Scale(Vector2 v, const Vector2& factors) noexcept { return v.Scale(factors); }

// Dot is a named function, not operator*: as an operator it reads as if it
// composes (a * b * c) when in fact the first product collapses to a scalar.
[[nodiscard]] constexpr float Dot(const Vector2& a, const Vector2& b) noexcept
{
	return a.x * b.x + a.y * b.y;
}

[[nodiscard]] constexpr float LengthSquared(const Vector2& v) noexcept { return Dot(v, v); }

// std::sqrt is not constexpr before C++26, so this one stays runtime-only.
[[nodiscard]] inline float Length(const Vector2& v) noexcept { return std::sqrt(LengthSquared(v)); }

// Returns the zero vector for a degenerate input rather than producing NaNs.
[[nodiscard]] inline Vector2 Normalized(const Vector2& v) noexcept
{
	const float lengthSq = LengthSquared(v);
	if (lengthSq <= 1e-12f)
		return {};

	return v * (1.0f / std::sqrt(lengthSq));
}

[[nodiscard]] inline bool NearlyEquals(const Vector2& a, const Vector2& b, float epsilon = 1e-5f) noexcept
{
	return std::fabs(a.x - b.x) <= epsilon
		&& std::fabs(a.y - b.y) <= epsilon;
}

// A 3-D Vector
struct Vector3
{
	float x = 0.0f;
	float y = 0.0f;
	float z = 0.0f;

	constexpr Vector3() = default;
	constexpr Vector3(float inX, float inY, float inZ) noexcept : x(inX), y(inY), z(inZ) {}

	// Contiguous XYZ for graphics APIs that take a float[3].
	[[nodiscard]] constexpr const float* Data() const noexcept { return &x; }
	[[nodiscard]] constexpr float* Data() noexcept { return &x; }

	constexpr Vector3& operator+=(const Vector3& rhs) noexcept { x += rhs.x; y += rhs.y; z += rhs.z; return *this; }
	constexpr Vector3& operator-=(const Vector3& rhs) noexcept { x -= rhs.x; y -= rhs.y; z -= rhs.z; return *this; }
	constexpr Vector3& operator*=(float scalar) noexcept { x *= scalar; y *= scalar; z *= scalar; return *this; }
	constexpr Vector3& operator/=(float scalar) noexcept
	{
		const float inv = 1.0f / scalar;
		x *= inv; y *= inv; z *= inv;
		return *this;
	}

	constexpr Vector3& Scale(float scalar) noexcept { return *this *= scalar; }

	constexpr Vector3& Scale(const Vector3& factors) noexcept
	{
		x *= factors.x; y *= factors.y; z *= factors.z;
		return *this;
	}

	[[nodiscard]] constexpr bool operator==(const Vector3&) const = default;
};

static_assert(sizeof(Vector3) == 3 * sizeof(float),
	"Vector3 must be tightly packed to alias as float[3].");

[[nodiscard]] constexpr Vector3 operator-(const Vector3& v) noexcept { return { -v.x, -v.y, -v.z }; }
[[nodiscard]] constexpr Vector3 operator+(const Vector3& v) noexcept { return v; }

[[nodiscard]] constexpr Vector3 operator+(Vector3 lhs, const Vector3& rhs) noexcept { return lhs += rhs; }
[[nodiscard]] constexpr Vector3 operator-(Vector3 lhs, const Vector3& rhs) noexcept { return lhs -= rhs; }

[[nodiscard]] constexpr Vector3 operator*(Vector3 v, float scalar) noexcept { return v *= scalar; }
[[nodiscard]] constexpr Vector3 operator*(float scalar, Vector3 v) noexcept { return v *= scalar; }
[[nodiscard]] constexpr Vector3 operator/(Vector3 v, float scalar) noexcept { return v /= scalar; }

[[nodiscard]] constexpr Vector3 Scale(Vector3 v, float scalar) noexcept { return v.Scale(scalar); }
[[nodiscard]] constexpr Vector3 Scale(Vector3 v, const Vector3& factors) noexcept { return v.Scale(factors); }

[[nodiscard]] constexpr float Dot(const Vector3& a, const Vector3& b) noexcept
{
	return a.x * b.x + a.y * b.y + a.z * b.z;
}

// Left-handed, matching the D3D convention used by the rest of the renderer.
[[nodiscard]] constexpr Vector3 Cross(const Vector3& a, const Vector3& b) noexcept
{
	return {
		a.y * b.z - a.z * b.y,
		a.z * b.x - a.x * b.z,
		a.x * b.y - a.y * b.x
	};
}

[[nodiscard]] constexpr float LengthSquared(const Vector3& v) noexcept { return Dot(v, v); }

[[nodiscard]] inline float Length(const Vector3& v) noexcept { return std::sqrt(LengthSquared(v)); }

[[nodiscard]] inline Vector3 Normalized(const Vector3& v) noexcept
{
	const float lengthSq = LengthSquared(v);
	if (lengthSq <= 1e-12f)
		return {};

	return v * (1.0f / std::sqrt(lengthSq));
}

[[nodiscard]] inline bool NearlyEquals(const Vector3& a, const Vector3& b, float epsilon = 1e-5f) noexcept
{
	return std::fabs(a.x - b.x) <= epsilon
		&& std::fabs(a.y - b.y) <= epsilon
		&& std::fabs(a.z - b.z) <= epsilon;
}

// A 4-D Vector
struct Vector4
{
	float x = 0.0f;
	float y = 0.0f;
	float z = 0.0f;
	float w = 0.0f;

	constexpr Vector4() = default;
	constexpr Vector4(float inX, float inY, float inZ, float inW) noexcept
		: x(inX), y(inY), z(inZ), w(inW) {}

	// Point (w = 1) and direction (w = 0) promotions, the two conversions that
	// actually come up when feeding a 4x4 transform.
	constexpr explicit Vector4(const Vector3& v, float inW = 1.0f) noexcept
		: x(v.x), y(v.y), z(v.z), w(inW) {}

	// Contiguous XYZW for graphics APIs that take a float[4].
	[[nodiscard]] constexpr const float* Data() const noexcept { return &x; }
	[[nodiscard]] constexpr float* Data() noexcept { return &x; }

	[[nodiscard]] constexpr Vector3 XYZ() const noexcept { return { x, y, z }; }

	constexpr Vector4& operator+=(const Vector4& rhs) noexcept { x += rhs.x; y += rhs.y; z += rhs.z; w += rhs.w; return *this; }
	constexpr Vector4& operator-=(const Vector4& rhs) noexcept { x -= rhs.x; y -= rhs.y; z -= rhs.z; w -= rhs.w; return *this; }
	constexpr Vector4& operator*=(float scalar) noexcept { x *= scalar; y *= scalar; z *= scalar; w *= scalar; return *this; }
	constexpr Vector4& operator/=(float scalar) noexcept
	{
		const float inv = 1.0f / scalar;
		x *= inv; y *= inv; z *= inv; w *= inv;
		return *this;
	}

	constexpr Vector4& Scale(float scalar) noexcept { return *this *= scalar; }

	constexpr Vector4& Scale(const Vector4& factors) noexcept
	{
		x *= factors.x; y *= factors.y; z *= factors.z; w *= factors.w;
		return *this;
	}

	// Scales xyz and leaves w alone -- the form you want on a position or
	// direction, where w is a tag rather than a magnitude.
	constexpr Vector4& Scale(const Vector3& factors) noexcept
	{
		x *= factors.x; y *= factors.y; z *= factors.z;
		return *this;
	}

	[[nodiscard]] constexpr bool operator==(const Vector4&) const = default;
};

static_assert(sizeof(Vector4) == 4 * sizeof(float),
	"Vector4 must be tightly packed to alias as float[4].");

[[nodiscard]] constexpr Vector4 operator-(const Vector4& v) noexcept { return { -v.x, -v.y, -v.z, -v.w }; }
[[nodiscard]] constexpr Vector4 operator+(const Vector4& v) noexcept { return v; }

[[nodiscard]] constexpr Vector4 operator+(Vector4 lhs, const Vector4& rhs) noexcept { return lhs += rhs; }
[[nodiscard]] constexpr Vector4 operator-(Vector4 lhs, const Vector4& rhs) noexcept { return lhs -= rhs; }

[[nodiscard]] constexpr Vector4 operator*(Vector4 v, float scalar) noexcept { return v *= scalar; }
[[nodiscard]] constexpr Vector4 operator*(float scalar, Vector4 v) noexcept { return v *= scalar; }
[[nodiscard]] constexpr Vector4 operator/(Vector4 v, float scalar) noexcept { return v /= scalar; }

[[nodiscard]] constexpr Vector4 Scale(Vector4 v, float scalar) noexcept { return v.Scale(scalar); }
[[nodiscard]] constexpr Vector4 Scale(Vector4 v, const Vector4& factors) noexcept { return v.Scale(factors); }
[[nodiscard]] constexpr Vector4 Scale(Vector4 v, const Vector3& factors) noexcept { return v.Scale(factors); }

[[nodiscard]] constexpr float Dot(const Vector4& a, const Vector4& b) noexcept
{
	return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
}

[[nodiscard]] constexpr float LengthSquared(const Vector4& v) noexcept { return Dot(v, v); }

[[nodiscard]] inline float Length(const Vector4& v) noexcept { return std::sqrt(LengthSquared(v)); }

[[nodiscard]] inline Vector4 Normalized(const Vector4& v) noexcept
{
	const float lengthSq = LengthSquared(v);
	if (lengthSq <= 1e-12f)
		return {};

	return v * (1.0f / std::sqrt(lengthSq));
}

[[nodiscard]] inline bool NearlyEquals(const Vector4& a, const Vector4& b, float epsilon = 1e-5f) noexcept
{
	return std::fabs(a.x - b.x) <= epsilon
		&& std::fabs(a.y - b.y) <= epsilon
		&& std::fabs(a.z - b.z) <= epsilon
		&& std::fabs(a.w - b.w) <= epsilon;
}
