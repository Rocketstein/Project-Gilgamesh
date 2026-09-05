#pragma once

#include "Vector.h"

#include <DirectXMath.h>

#include <limits>
#include <optional>

// Row-major storage (m[row][col]) with the row-vector convention: vectors are
// rows, transforms apply as v' = v * M, and translation lives in the last row.
// This uses the same element order as DirectXMath's storage types, and the
// composition order reads left to right -- A * B means "apply A, then B".
// Use the explicit ToDirectX / FromDirectX helpers below instead of type-punning
// these objects as XMFLOAT matrices.
//
// HLSL defaults to column-major, so a matrix bound to a constant buffer needs
// a Transpose on the way in unless the shader declares row_major.

namespace MatrixDetail
{
	[[nodiscard]] constexpr bool IsFinite(float value) noexcept
	{
		return value == value
			&& value <= std::numeric_limits<float>::max()
			&& value >= -std::numeric_limits<float>::max();
	}
}

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

// False leaves out untouched. A fixed determinant epsilon is intentionally not
// used because it is not scale-independent; callers that care about conditioning
// should apply a policy appropriate for their data.
[[nodiscard]] constexpr bool TryInverse(const Matrix2& a, Matrix2& out) noexcept
{
	const float det = Determinant(a);
	if (det == 0.0f || !MatrixDetail::IsFinite(det))
		return false;

	const float invDet = 1.0f / det;
	const Matrix2 candidate{
		 a.m[1][1] * invDet, -a.m[0][1] * invDet,
		-a.m[1][0] * invDet,  a.m[0][0] * invDet
	};
	if (!MatrixDetail::IsFinite(candidate.m[0][0])
		|| !MatrixDetail::IsFinite(candidate.m[0][1])
		|| !MatrixDetail::IsFinite(candidate.m[1][0])
		|| !MatrixDetail::IsFinite(candidate.m[1][1]))
	{
		return false;
	}

	out = candidate;
	return true;
}

[[nodiscard]] constexpr std::optional<Matrix2> Inverse(const Matrix2& a) noexcept
{
	Matrix2 result;
	if (!TryInverse(a, result))
		return std::nullopt;
	return result;
}

[[nodiscard]] inline bool NearlyEquals(const Matrix2& a, const Matrix2& b, float epsilon = 1e-5f) noexcept
{
	if (epsilon < 0.0f || !MatrixDetail::IsFinite(epsilon))
		return false;

	for (int r = 0; r < 2; ++r)
		for (int c = 0; c < 2; ++c)
			if (!(std::fabs(a.m[r][c] - b.m[r][c]) <= epsilon))
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
	if (det == 0.0f || !MatrixDetail::IsFinite(det))
		return false;

	const float c10 = -(a.m[0][1] * a.m[2][2] - a.m[0][2] * a.m[2][1]);
	const float c11 =  (a.m[0][0] * a.m[2][2] - a.m[0][2] * a.m[2][0]);
	const float c12 = -(a.m[0][0] * a.m[2][1] - a.m[0][1] * a.m[2][0]);
	const float c20 =  (a.m[0][1] * a.m[1][2] - a.m[0][2] * a.m[1][1]);
	const float c21 = -(a.m[0][0] * a.m[1][2] - a.m[0][2] * a.m[1][0]);
	const float c22 =  (a.m[0][0] * a.m[1][1] - a.m[0][1] * a.m[1][0]);

	const float invDet = 1.0f / det;
	const Matrix candidate{
		c00 * invDet, c10 * invDet, c20 * invDet,
		c01 * invDet, c11 * invDet, c21 * invDet,
		c02 * invDet, c12 * invDet, c22 * invDet
	};
	for (int r = 0; r < 3; ++r)
		for (int c = 0; c < 3; ++c)
			if (!MatrixDetail::IsFinite(candidate.m[r][c]))
				return false;

	out = candidate;
	return true;
}

[[nodiscard]] constexpr std::optional<Matrix> Inverse(const Matrix& a) noexcept
{
	Matrix result;
	if (!TryInverse(a, result))
		return std::nullopt;
	return result;
}

[[nodiscard]] inline bool NearlyEquals(const Matrix& a, const Matrix& b, float epsilon = 1e-5f) noexcept
{
	if (epsilon < 0.0f || !MatrixDetail::IsFinite(epsilon))
		return false;

	for (int r = 0; r < 3; ++r)
		for (int c = 0; c < 3; ++c)
			if (!(std::fabs(a.m[r][c] - b.m[r][c]) <= epsilon))
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

	Matrix4& operator+=(const Matrix4& rhs) noexcept;
	Matrix4& operator-=(const Matrix4& rhs) noexcept;
	Matrix4& operator*=(float scalar) noexcept;

	[[nodiscard]] constexpr bool operator==(const Matrix4&) const = default;
};

static_assert(sizeof(Matrix4) == 16 * sizeof(float),
	"Matrix4 must be tightly packed as sixteen floats.");

// Convert through actual XMFLOAT4X4 objects so callers never need to rely on
// pointer reinterpretation or strict-aliasing exceptions.
[[nodiscard]] inline DirectX::XMMATRIX ToDirectX(const Matrix4& matrix) noexcept
{
	const DirectX::XMFLOAT4X4 storage{
		matrix.m[0][0], matrix.m[0][1], matrix.m[0][2], matrix.m[0][3],
		matrix.m[1][0], matrix.m[1][1], matrix.m[1][2], matrix.m[1][3],
		matrix.m[2][0], matrix.m[2][1], matrix.m[2][2], matrix.m[2][3],
		matrix.m[3][0], matrix.m[3][1], matrix.m[3][2], matrix.m[3][3]
	};
	return DirectX::XMLoadFloat4x4(&storage);
}

[[nodiscard]] inline Matrix4 FromDirectX(DirectX::FXMMATRIX matrix) noexcept
{
	DirectX::XMFLOAT4X4 storage;
	DirectX::XMStoreFloat4x4(&storage, matrix);
	return {
		storage._11, storage._12, storage._13, storage._14,
		storage._21, storage._22, storage._23, storage._24,
		storage._31, storage._32, storage._33, storage._34,
		storage._41, storage._42, storage._43, storage._44
	};
}

inline Matrix4& Matrix4::operator+=(const Matrix4& rhs) noexcept
{
	return *this = FromDirectX(ToDirectX(*this) + ToDirectX(rhs));
}

inline Matrix4& Matrix4::operator-=(const Matrix4& rhs) noexcept
{
	return *this = FromDirectX(ToDirectX(*this) - ToDirectX(rhs));
}

inline Matrix4& Matrix4::operator*=(float scalar) noexcept
{
	return *this = FromDirectX(ToDirectX(*this) * scalar);
}

[[nodiscard]] inline Matrix4 operator+(Matrix4 lhs, const Matrix4& rhs) noexcept { return lhs += rhs; }
[[nodiscard]] inline Matrix4 operator-(Matrix4 lhs, const Matrix4& rhs) noexcept { return lhs -= rhs; }
[[nodiscard]] inline Matrix4 operator*(Matrix4 a, float scalar) noexcept { return a *= scalar; }
[[nodiscard]] inline Matrix4 operator*(float scalar, Matrix4 a) noexcept { return a *= scalar; }

// A * B applies A first, then B.
[[nodiscard]] inline Matrix4 operator*(const Matrix4& a, const Matrix4& b) noexcept
{
	return FromDirectX(DirectX::XMMatrixMultiply(ToDirectX(a), ToDirectX(b)));
}

inline Matrix4& operator*=(Matrix4& a, const Matrix4& b) noexcept { return a = a * b; }

[[nodiscard]] inline Vector4 operator*(const Vector4& v, const Matrix4& a) noexcept
{
	const DirectX::XMVECTOR vector = DirectX::XMVectorSet(v.x, v.y, v.z, v.w);
	DirectX::XMFLOAT4 result;
	DirectX::XMStoreFloat4(
		&result,
		DirectX::XMVector4Transform(vector, ToDirectX(a)));
	return { result.x, result.y, result.z, result.w };
}

// w = 1, so translation applies. Assumes an affine matrix -- no perspective
// divide. Go through Vector4 for a projection matrix.
[[nodiscard]] inline Vector3 TransformPoint(const Vector3& v, const Matrix4& a) noexcept
{
	const DirectX::XMVECTOR vector = DirectX::XMVectorSet(v.x, v.y, v.z, 1.0f);
	DirectX::XMFLOAT3 result;
	DirectX::XMStoreFloat3(
		&result,
		DirectX::XMVector3Transform(vector, ToDirectX(a)));
	return { result.x, result.y, result.z };
}

// w = 0, so translation does not apply.
[[nodiscard]] inline Vector3 TransformDirection(const Vector3& v, const Matrix4& a) noexcept
{
	const DirectX::XMVECTOR vector = DirectX::XMVectorSet(v.x, v.y, v.z, 0.0f);
	DirectX::XMFLOAT3 result;
	DirectX::XMStoreFloat3(
		&result,
		DirectX::XMVector3TransformNormal(vector, ToDirectX(a)));
	return { result.x, result.y, result.z };
}

[[nodiscard]] inline Matrix4 Transpose(const Matrix4& a) noexcept
{
	return FromDirectX(DirectX::XMMatrixTranspose(ToDirectX(a)));
}

[[nodiscard]] inline float Determinant(const Matrix4& a) noexcept
{
	return DirectX::XMVectorGetX(DirectX::XMMatrixDeterminant(ToDirectX(a)));
}

[[nodiscard]] inline bool TryInverse(const Matrix4& a, Matrix4& out) noexcept
{
	const DirectX::XMMATRIX matrix = ToDirectX(a);
	if (DirectX::XMMatrixIsNaN(matrix) || DirectX::XMMatrixIsInfinite(matrix))
		return false;

	DirectX::XMVECTOR determinant;
	const DirectX::XMMATRIX inverse = DirectX::XMMatrixInverse(&determinant, matrix);
	const float det = DirectX::XMVectorGetX(determinant);
	if (det == 0.0f || !MatrixDetail::IsFinite(det)
		|| DirectX::XMMatrixIsNaN(inverse)
		|| DirectX::XMMatrixIsInfinite(inverse))
	{
		return false;
	}

	out = FromDirectX(inverse);
	return true;
}

[[nodiscard]] inline std::optional<Matrix4> Inverse(const Matrix4& a) noexcept
{
	Matrix4 result;
	if (!TryInverse(a, result))
		return std::nullopt;
	return result;
}

[[nodiscard]] inline bool NearlyEquals(const Matrix4& a, const Matrix4& b, float epsilon = 1e-5f) noexcept
{
	if (epsilon < 0.0f || !MatrixDetail::IsFinite(epsilon))
		return false;

	for (int r = 0; r < 4; ++r)
		for (int c = 0; c < 4; ++c)
			if (!(std::fabs(a.m[r][c] - b.m[r][c]) <= epsilon))
				return false;
	return true;
}
