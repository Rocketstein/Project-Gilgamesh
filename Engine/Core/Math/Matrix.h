#pragma once

#include "Vector.h"

// Row-major storage (m[row][col]) with the row-vector convention: vectors are
// rows, transforms apply as v' = v * M, and translation lives in the last row.
// This is DirectXMath's layout, so these alias XMFLOAT3X3 / XMFLOAT4X4 and the
// composition order reads left to right -- A * B means "apply A, then B".
//
// HLSL defaults to column-major, so a matrix bound to a constant buffer needs
// a Transpose on the way in unless the shader declares row_major.

// A 2x2 matrix
struct Matrix2
{
	// Defaults to identity.
	float m[2][2] = {
		{ 1.0f, 0.0f },
		{ 0.0f, 1.0f }
	};

	constexpr Matrix2() = default;
	constexpr Matrix2(float m00, float m01,
	                  float m10, float m11) noexcept
		: m{ { m00, m01 },
		     { m10, m11 } } {}

	// Rows, in the order they multiply.
	constexpr Matrix2(const Vector2& row0, const Vector2& row1) noexcept
		: m{ { row0.x, row0.y },
		     { row1.x, row1.y } } {}

	[[nodiscard]] static constexpr Matrix2 Identity() noexcept { return {}; }

	[[nodiscard]] constexpr float& operator()(int row, int col) noexcept { return m[row][col]; }
	[[nodiscard]] constexpr float operator()(int row, int col) const noexcept { return m[row][col]; }

	[[nodiscard]] constexpr Vector2 Row(int index) const noexcept { return { m[index][0], m[index][1] }; }
	[[nodiscard]] constexpr Vector2 Column(int index) const noexcept { return { m[0][index], m[1][index] }; }

	// Contiguous for graphics APIs that take a float[4].
	[[nodiscard]] constexpr const float* Data() const noexcept { return &m[0][0]; }
	[[nodiscard]] constexpr float* Data() noexcept { return &m[0][0]; }

	constexpr Matrix2& operator+=(const Matrix2& rhs) noexcept
	{
		for (int r = 0; r < 2; ++r)
			for (int c = 0; c < 2; ++c)
				m[r][c] += rhs.m[r][c];
		return *this;
	}

	constexpr Matrix2& operator-=(const Matrix2& rhs) noexcept
	{
		for (int r = 0; r < 2; ++r)
			for (int c = 0; c < 2; ++c)
				m[r][c] -= rhs.m[r][c];
		return *this;
	}

	constexpr Matrix2& operator*=(float scalar) noexcept
	{
		for (int r = 0; r < 2; ++r)
			for (int c = 0; c < 2; ++c)
				m[r][c] *= scalar;
		return *this;
	}

	[[nodiscard]] constexpr bool operator==(const Matrix2&) const = default;
};

static_assert(sizeof(Matrix2) == 4 * sizeof(float),
	"Matrix2 must be tightly packed to alias as float[4].");

[[nodiscard]] constexpr Matrix2 operator+(Matrix2 lhs, const Matrix2& rhs) noexcept { return lhs += rhs; }
[[nodiscard]] constexpr Matrix2 operator-(Matrix2 lhs, const Matrix2& rhs) noexcept { return lhs -= rhs; }
[[nodiscard]] constexpr Matrix2 operator*(Matrix2 a, float scalar) noexcept { return a *= scalar; }
[[nodiscard]] constexpr Matrix2 operator*(float scalar, Matrix2 a) noexcept { return a *= scalar; }

[[nodiscard]] constexpr Matrix2 operator*(const Matrix2& a, const Matrix2& b) noexcept
{
	Matrix2 result;
	for (int r = 0; r < 2; ++r)
		for (int c = 0; c < 2; ++c)
			result.m[r][c] = a.m[r][0] * b.m[0][c]
			               + a.m[r][1] * b.m[1][c];
	return result;
}

constexpr Matrix2& operator*=(Matrix2& a, const Matrix2& b) noexcept { return a = a * b; }

// Row vector times matrix -- the only ordering this convention defines.
[[nodiscard]] constexpr Vector2 operator*(const Vector2& v, const Matrix2& a) noexcept
{
	return {
		v.x * a.m[0][0] + v.y * a.m[1][0],
		v.x * a.m[0][1] + v.y * a.m[1][1]
	};
}

[[nodiscard]] constexpr Matrix2 Transpose(const Matrix2& a) noexcept
{
	return {
		a.m[0][0], a.m[1][0],
		a.m[0][1], a.m[1][1]
	};
}

[[nodiscard]] constexpr float Determinant(const Matrix2& a) noexcept
{
	return a.m[0][0] * a.m[1][1] - a.m[0][1] * a.m[1][0];
}

// False leaves out untouched. Rejects only an exactly singular matrix: a small
// determinant is legitimate (a uniformly scaled-down transform has one) and
// still inverts correctly, so an epsilon here would reject valid input.
[[nodiscard]] constexpr bool TryInverse(const Matrix2& a, Matrix2& out) noexcept
{
	const float det = Determinant(a);
	if (det == 0.0f)
		return false;

	const float invDet = 1.0f / det;
	out = {
		 a.m[1][1] * invDet, -a.m[0][1] * invDet,
		-a.m[1][0] * invDet,  a.m[0][0] * invDet
	};
	return true;
}

// Identity on a singular matrix. Use TryInverse when that has to be detected.
[[nodiscard]] constexpr Matrix2 Inverse(const Matrix2& a) noexcept
{
	Matrix2 result;
	static_cast<void>(TryInverse(a, result));	// Failure leaves the identity.
	return result;
}

[[nodiscard]] inline bool NearlyEquals(const Matrix2& a, const Matrix2& b, float epsilon = 1e-5f) noexcept
{
	for (int r = 0; r < 2; ++r)
		for (int c = 0; c < 2; ++c)
			if (std::fabs(a.m[r][c] - b.m[r][c]) > epsilon)
				return false;
	return true;
}

// A 3x3 matrix
struct Matrix
{
	// Defaults to identity.
	float m[3][3] = {
		{ 1.0f, 0.0f, 0.0f },
		{ 0.0f, 1.0f, 0.0f },
		{ 0.0f, 0.0f, 1.0f }
	};

	constexpr Matrix() = default;
	constexpr Matrix(float m00, float m01, float m02,
	                 float m10, float m11, float m12,
	                 float m20, float m21, float m22) noexcept
		: m{ { m00, m01, m02 },
		     { m10, m11, m12 },
		     { m20, m21, m22 } } {}

	constexpr Matrix(const Vector3& row0, const Vector3& row1, const Vector3& row2) noexcept
		: m{ { row0.x, row0.y, row0.z },
		     { row1.x, row1.y, row1.z },
		     { row2.x, row2.y, row2.z } } {}

	[[nodiscard]] static constexpr Matrix Identity() noexcept { return {}; }

	[[nodiscard]] constexpr float& operator()(int row, int col) noexcept { return m[row][col]; }
	[[nodiscard]] constexpr float operator()(int row, int col) const noexcept { return m[row][col]; }

	[[nodiscard]] constexpr Vector3 Row(int index) const noexcept { return { m[index][0], m[index][1], m[index][2] }; }
	[[nodiscard]] constexpr Vector3 Column(int index) const noexcept { return { m[0][index], m[1][index], m[2][index] }; }

	// Contiguous for graphics APIs that take a float[9].
	[[nodiscard]] constexpr const float* Data() const noexcept { return &m[0][0]; }
	[[nodiscard]] constexpr float* Data() noexcept { return &m[0][0]; }

	constexpr Matrix& operator+=(const Matrix& rhs) noexcept
	{
		for (int r = 0; r < 3; ++r)
			for (int c = 0; c < 3; ++c)
				m[r][c] += rhs.m[r][c];
		return *this;
	}

	constexpr Matrix& operator-=(const Matrix& rhs) noexcept
	{
		for (int r = 0; r < 3; ++r)
			for (int c = 0; c < 3; ++c)
				m[r][c] -= rhs.m[r][c];
		return *this;
	}

	constexpr Matrix& operator*=(float scalar) noexcept
	{
		for (int r = 0; r < 3; ++r)
			for (int c = 0; c < 3; ++c)
				m[r][c] *= scalar;
		return *this;
	}

	[[nodiscard]] constexpr bool operator==(const Matrix&) const = default;
};

static_assert(sizeof(Matrix) == 9 * sizeof(float),
	"Matrix must be tightly packed to alias as float[9].");

[[nodiscard]] constexpr Matrix operator+(Matrix lhs, const Matrix& rhs) noexcept { return lhs += rhs; }
[[nodiscard]] constexpr Matrix operator-(Matrix lhs, const Matrix& rhs) noexcept { return lhs -= rhs; }
[[nodiscard]] constexpr Matrix operator*(Matrix a, float scalar) noexcept { return a *= scalar; }
[[nodiscard]] constexpr Matrix operator*(float scalar, Matrix a) noexcept { return a *= scalar; }

[[nodiscard]] constexpr Matrix operator*(const Matrix& a, const Matrix& b) noexcept
{
	Matrix result;
	for (int r = 0; r < 3; ++r)
		for (int c = 0; c < 3; ++c)
			result.m[r][c] = a.m[r][0] * b.m[0][c]
			               + a.m[r][1] * b.m[1][c]
			               + a.m[r][2] * b.m[2][c];
	return result;
}

constexpr Matrix& operator*=(Matrix& a, const Matrix& b) noexcept { return a = a * b; }

[[nodiscard]] constexpr Vector3 operator*(const Vector3& v, const Matrix& a) noexcept
{
	return {
		v.x * a.m[0][0] + v.y * a.m[1][0] + v.z * a.m[2][0],
		v.x * a.m[0][1] + v.y * a.m[1][1] + v.z * a.m[2][1],
		v.x * a.m[0][2] + v.y * a.m[1][2] + v.z * a.m[2][2]
	};
}

[[nodiscard]] constexpr Matrix Transpose(const Matrix& a) noexcept
{
	return {
		a.m[0][0], a.m[1][0], a.m[2][0],
		a.m[0][1], a.m[1][1], a.m[2][1],
		a.m[0][2], a.m[1][2], a.m[2][2]
	};
}

[[nodiscard]] constexpr float Determinant(const Matrix& a) noexcept
{
	return a.m[0][0] * (a.m[1][1] * a.m[2][2] - a.m[1][2] * a.m[2][1])
	     - a.m[0][1] * (a.m[1][0] * a.m[2][2] - a.m[1][2] * a.m[2][0])
	     + a.m[0][2] * (a.m[1][0] * a.m[2][1] - a.m[1][1] * a.m[2][0]);
}

// Adjugate over determinant. The adjugate is the transpose of the cofactor
// matrix, which is why the indices below look mirrored.
[[nodiscard]] constexpr bool TryInverse(const Matrix& a, Matrix& out) noexcept
{
	const float c00 =  (a.m[1][1] * a.m[2][2] - a.m[1][2] * a.m[2][1]);
	const float c01 = -(a.m[1][0] * a.m[2][2] - a.m[1][2] * a.m[2][0]);
	const float c02 =  (a.m[1][0] * a.m[2][1] - a.m[1][1] * a.m[2][0]);

	const float det = a.m[0][0] * c00 + a.m[0][1] * c01 + a.m[0][2] * c02;
	if (det == 0.0f)
		return false;

	const float c10 = -(a.m[0][1] * a.m[2][2] - a.m[0][2] * a.m[2][1]);
	const float c11 =  (a.m[0][0] * a.m[2][2] - a.m[0][2] * a.m[2][0]);
	const float c12 = -(a.m[0][0] * a.m[2][1] - a.m[0][1] * a.m[2][0]);
	const float c20 =  (a.m[0][1] * a.m[1][2] - a.m[0][2] * a.m[1][1]);
	const float c21 = -(a.m[0][0] * a.m[1][2] - a.m[0][2] * a.m[1][0]);
	const float c22 =  (a.m[0][0] * a.m[1][1] - a.m[0][1] * a.m[1][0]);

	const float invDet = 1.0f / det;
	out = {
		c00 * invDet, c10 * invDet, c20 * invDet,
		c01 * invDet, c11 * invDet, c21 * invDet,
		c02 * invDet, c12 * invDet, c22 * invDet
	};
	return true;
}

// Identity on a singular matrix. Use TryInverse when that has to be detected.
[[nodiscard]] constexpr Matrix Inverse(const Matrix& a) noexcept
{
	Matrix result;
	static_cast<void>(TryInverse(a, result));	// Failure leaves the identity.
	return result;
}

[[nodiscard]] inline bool NearlyEquals(const Matrix& a, const Matrix& b, float epsilon = 1e-5f) noexcept
{
	for (int r = 0; r < 3; ++r)
		for (int c = 0; c < 3; ++c)
			if (std::fabs(a.m[r][c] - b.m[r][c]) > epsilon)
				return false;
	return true;
}

// A 4x4 matrix
struct Matrix4
{
	// Defaults to identity. Translation occupies row 3.
	float m[4][4] = {
		{ 1.0f, 0.0f, 0.0f, 0.0f },
		{ 0.0f, 1.0f, 0.0f, 0.0f },
		{ 0.0f, 0.0f, 1.0f, 0.0f },
		{ 0.0f, 0.0f, 0.0f, 1.0f }
	};

	constexpr Matrix4() = default;
	constexpr Matrix4(float m00, float m01, float m02, float m03,
	                  float m10, float m11, float m12, float m13,
	                  float m20, float m21, float m22, float m23,
	                  float m30, float m31, float m32, float m33) noexcept
		: m{ { m00, m01, m02, m03 },
		     { m10, m11, m12, m13 },
		     { m20, m21, m22, m23 },
		     { m30, m31, m32, m33 } } {}

	constexpr Matrix4(const Vector4& row0, const Vector4& row1,
	                  const Vector4& row2, const Vector4& row3) noexcept
		: m{ { row0.x, row0.y, row0.z, row0.w },
		     { row1.x, row1.y, row1.z, row1.w },
		     { row2.x, row2.y, row2.z, row2.w },
		     { row3.x, row3.y, row3.z, row3.w } } {}

	// Upper-left 3x3 plus a translation row.
	constexpr explicit Matrix4(const Matrix& basis, const Vector3& translation = {}) noexcept
		: m{ { basis.m[0][0], basis.m[0][1], basis.m[0][2], 0.0f },
		     { basis.m[1][0], basis.m[1][1], basis.m[1][2], 0.0f },
		     { basis.m[2][0], basis.m[2][1], basis.m[2][2], 0.0f },
		     { translation.x, translation.y, translation.z, 1.0f } } {}

	[[nodiscard]] static constexpr Matrix4 Identity() noexcept { return {}; }

	[[nodiscard]] constexpr float& operator()(int row, int col) noexcept { return m[row][col]; }
	[[nodiscard]] constexpr float operator()(int row, int col) const noexcept { return m[row][col]; }

	[[nodiscard]] constexpr Vector4 Row(int index) const noexcept { return { m[index][0], m[index][1], m[index][2], m[index][3] }; }
	[[nodiscard]] constexpr Vector4 Column(int index) const noexcept { return { m[0][index], m[1][index], m[2][index], m[3][index] }; }

	// Rotation-and-scale block, without the translation row.
	[[nodiscard]] constexpr Matrix Basis() const noexcept
	{
		return {
			m[0][0], m[0][1], m[0][2],
			m[1][0], m[1][1], m[1][2],
			m[2][0], m[2][1], m[2][2]
		};
	}

	[[nodiscard]] constexpr Vector3 Translation() const noexcept { return { m[3][0], m[3][1], m[3][2] }; }

	// Contiguous for graphics APIs that take a float[16].
	[[nodiscard]] constexpr const float* Data() const noexcept { return &m[0][0]; }
	[[nodiscard]] constexpr float* Data() noexcept { return &m[0][0]; }

	constexpr Matrix4& operator+=(const Matrix4& rhs) noexcept
	{
		for (int r = 0; r < 4; ++r)
			for (int c = 0; c < 4; ++c)
				m[r][c] += rhs.m[r][c];
		return *this;
	}

	constexpr Matrix4& operator-=(const Matrix4& rhs) noexcept
	{
		for (int r = 0; r < 4; ++r)
			for (int c = 0; c < 4; ++c)
				m[r][c] -= rhs.m[r][c];
		return *this;
	}

	constexpr Matrix4& operator*=(float scalar) noexcept
	{
		for (int r = 0; r < 4; ++r)
			for (int c = 0; c < 4; ++c)
				m[r][c] *= scalar;
		return *this;
	}

	[[nodiscard]] constexpr bool operator==(const Matrix4&) const = default;
};

static_assert(sizeof(Matrix4) == 16 * sizeof(float),
	"Matrix4 must be tightly packed to alias as float[16] / XMFLOAT4X4.");

[[nodiscard]] constexpr Matrix4 operator+(Matrix4 lhs, const Matrix4& rhs) noexcept { return lhs += rhs; }
[[nodiscard]] constexpr Matrix4 operator-(Matrix4 lhs, const Matrix4& rhs) noexcept { return lhs -= rhs; }
[[nodiscard]] constexpr Matrix4 operator*(Matrix4 a, float scalar) noexcept { return a *= scalar; }
[[nodiscard]] constexpr Matrix4 operator*(float scalar, Matrix4 a) noexcept { return a *= scalar; }

// A * B applies A first, then B.
[[nodiscard]] constexpr Matrix4 operator*(const Matrix4& a, const Matrix4& b) noexcept
{
	Matrix4 result;
	for (int r = 0; r < 4; ++r)
		for (int c = 0; c < 4; ++c)
			result.m[r][c] = a.m[r][0] * b.m[0][c]
			               + a.m[r][1] * b.m[1][c]
			               + a.m[r][2] * b.m[2][c]
			               + a.m[r][3] * b.m[3][c];
	return result;
}

constexpr Matrix4& operator*=(Matrix4& a, const Matrix4& b) noexcept { return a = a * b; }

[[nodiscard]] constexpr Vector4 operator*(const Vector4& v, const Matrix4& a) noexcept
{
	return {
		v.x * a.m[0][0] + v.y * a.m[1][0] + v.z * a.m[2][0] + v.w * a.m[3][0],
		v.x * a.m[0][1] + v.y * a.m[1][1] + v.z * a.m[2][1] + v.w * a.m[3][1],
		v.x * a.m[0][2] + v.y * a.m[1][2] + v.z * a.m[2][2] + v.w * a.m[3][2],
		v.x * a.m[0][3] + v.y * a.m[1][3] + v.z * a.m[2][3] + v.w * a.m[3][3]
	};
}

// w = 1, so translation applies. Assumes an affine matrix -- no perspective
// divide. Go through Vector4 for a projection matrix.
[[nodiscard]] constexpr Vector3 TransformPoint(const Vector3& v, const Matrix4& a) noexcept
{
	return {
		v.x * a.m[0][0] + v.y * a.m[1][0] + v.z * a.m[2][0] + a.m[3][0],
		v.x * a.m[0][1] + v.y * a.m[1][1] + v.z * a.m[2][1] + a.m[3][1],
		v.x * a.m[0][2] + v.y * a.m[1][2] + v.z * a.m[2][2] + a.m[3][2]
	};
}

// w = 0, so translation does not apply.
[[nodiscard]] constexpr Vector3 TransformDirection(const Vector3& v, const Matrix4& a) noexcept
{
	return {
		v.x * a.m[0][0] + v.y * a.m[1][0] + v.z * a.m[2][0],
		v.x * a.m[0][1] + v.y * a.m[1][1] + v.z * a.m[2][1],
		v.x * a.m[0][2] + v.y * a.m[1][2] + v.z * a.m[2][2]
	};
}

[[nodiscard]] constexpr Matrix4 Transpose(const Matrix4& a) noexcept
{
	return {
		a.m[0][0], a.m[1][0], a.m[2][0], a.m[3][0],
		a.m[0][1], a.m[1][1], a.m[2][1], a.m[3][1],
		a.m[0][2], a.m[1][2], a.m[2][2], a.m[3][2],
		a.m[0][3], a.m[1][3], a.m[2][3], a.m[3][3]
	};
}

// Laplace expansion on 2x2 minors: s* come from the top two rows, c* from the
// bottom two. Sharing them is what keeps this cheaper than nine 3x3 dets.
[[nodiscard]] constexpr float Determinant(const Matrix4& a) noexcept
{
	const float s0 = a.m[0][0] * a.m[1][1] - a.m[1][0] * a.m[0][1];
	const float s1 = a.m[0][0] * a.m[1][2] - a.m[1][0] * a.m[0][2];
	const float s2 = a.m[0][0] * a.m[1][3] - a.m[1][0] * a.m[0][3];
	const float s3 = a.m[0][1] * a.m[1][2] - a.m[1][1] * a.m[0][2];
	const float s4 = a.m[0][1] * a.m[1][3] - a.m[1][1] * a.m[0][3];
	const float s5 = a.m[0][2] * a.m[1][3] - a.m[1][2] * a.m[0][3];

	const float c5 = a.m[2][2] * a.m[3][3] - a.m[3][2] * a.m[2][3];
	const float c4 = a.m[2][1] * a.m[3][3] - a.m[3][1] * a.m[2][3];
	const float c3 = a.m[2][1] * a.m[3][2] - a.m[3][1] * a.m[2][2];
	const float c2 = a.m[2][0] * a.m[3][3] - a.m[3][0] * a.m[2][3];
	const float c1 = a.m[2][0] * a.m[3][2] - a.m[3][0] * a.m[2][2];
	const float c0 = a.m[2][0] * a.m[3][1] - a.m[3][0] * a.m[2][1];

	return s0 * c5 - s1 * c4 + s2 * c3 + s3 * c2 - s4 * c1 + s5 * c0;
}

[[nodiscard]] constexpr bool TryInverse(const Matrix4& a, Matrix4& out) noexcept
{
	const float s0 = a.m[0][0] * a.m[1][1] - a.m[1][0] * a.m[0][1];
	const float s1 = a.m[0][0] * a.m[1][2] - a.m[1][0] * a.m[0][2];
	const float s2 = a.m[0][0] * a.m[1][3] - a.m[1][0] * a.m[0][3];
	const float s3 = a.m[0][1] * a.m[1][2] - a.m[1][1] * a.m[0][2];
	const float s4 = a.m[0][1] * a.m[1][3] - a.m[1][1] * a.m[0][3];
	const float s5 = a.m[0][2] * a.m[1][3] - a.m[1][2] * a.m[0][3];

	const float c5 = a.m[2][2] * a.m[3][3] - a.m[3][2] * a.m[2][3];
	const float c4 = a.m[2][1] * a.m[3][3] - a.m[3][1] * a.m[2][3];
	const float c3 = a.m[2][1] * a.m[3][2] - a.m[3][1] * a.m[2][2];
	const float c2 = a.m[2][0] * a.m[3][3] - a.m[3][0] * a.m[2][3];
	const float c1 = a.m[2][0] * a.m[3][2] - a.m[3][0] * a.m[2][2];
	const float c0 = a.m[2][0] * a.m[3][1] - a.m[3][0] * a.m[2][1];

	const float det = s0 * c5 - s1 * c4 + s2 * c3 + s3 * c2 - s4 * c1 + s5 * c0;
	if (det == 0.0f)
		return false;

	const float invDet = 1.0f / det;

	out.m[0][0] = ( a.m[1][1] * c5 - a.m[1][2] * c4 + a.m[1][3] * c3) * invDet;
	out.m[0][1] = (-a.m[0][1] * c5 + a.m[0][2] * c4 - a.m[0][3] * c3) * invDet;
	out.m[0][2] = ( a.m[3][1] * s5 - a.m[3][2] * s4 + a.m[3][3] * s3) * invDet;
	out.m[0][3] = (-a.m[2][1] * s5 + a.m[2][2] * s4 - a.m[2][3] * s3) * invDet;

	out.m[1][0] = (-a.m[1][0] * c5 + a.m[1][2] * c2 - a.m[1][3] * c1) * invDet;
	out.m[1][1] = ( a.m[0][0] * c5 - a.m[0][2] * c2 + a.m[0][3] * c1) * invDet;
	out.m[1][2] = (-a.m[3][0] * s5 + a.m[3][2] * s2 - a.m[3][3] * s1) * invDet;
	out.m[1][3] = ( a.m[2][0] * s5 - a.m[2][2] * s2 + a.m[2][3] * s1) * invDet;

	out.m[2][0] = ( a.m[1][0] * c4 - a.m[1][1] * c2 + a.m[1][3] * c0) * invDet;
	out.m[2][1] = (-a.m[0][0] * c4 + a.m[0][1] * c2 - a.m[0][3] * c0) * invDet;
	out.m[2][2] = ( a.m[3][0] * s4 - a.m[3][1] * s2 + a.m[3][3] * s0) * invDet;
	out.m[2][3] = (-a.m[2][0] * s4 + a.m[2][1] * s2 - a.m[2][3] * s0) * invDet;

	out.m[3][0] = (-a.m[1][0] * c3 + a.m[1][1] * c1 - a.m[1][2] * c0) * invDet;
	out.m[3][1] = ( a.m[0][0] * c3 - a.m[0][1] * c1 + a.m[0][2] * c0) * invDet;
	out.m[3][2] = (-a.m[3][0] * s3 + a.m[3][1] * s1 - a.m[3][2] * s0) * invDet;
	out.m[3][3] = ( a.m[2][0] * s3 - a.m[2][1] * s1 + a.m[2][2] * s0) * invDet;

	return true;
}

// Identity on a singular matrix. Use TryInverse when that has to be detected.
[[nodiscard]] constexpr Matrix4 Inverse(const Matrix4& a) noexcept
{
	Matrix4 result;
	static_cast<void>(TryInverse(a, result));	// Failure leaves the identity.
	return result;
}

[[nodiscard]] inline bool NearlyEquals(const Matrix4& a, const Matrix4& b, float epsilon = 1e-5f) noexcept
{
	for (int r = 0; r < 4; ++r)
		for (int c = 0; c < 4; ++c)
			if (std::fabs(a.m[r][c] - b.m[r][c]) > epsilon)
				return false;
	return true;
}
