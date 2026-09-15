#pragma once

#include "Quat.h"

#include <algorithm>
#include <cmath>
#include <numbers>

// Human-facing Euler rotation stored in degrees. Internal rotation math should
// use Quat so repeated composition does not inherit Euler-angle singularities.
//
// Gilgamesh is left-handed and Z-up: +X is forward, +Y is right, and +Z is up.
// Positive yaw turns right around +Z, positive pitch looks up around -Y, and
// positive roll banks right around -X.

namespace RotatorUtils
{
	inline constexpr float FullTurnDegrees = 360.0f;
	inline constexpr float DegreesToRadiansScale =
		std::numbers::pi_v<float> / 180.0f;
	inline constexpr float RadiansToDegreesScale =
		180.0f / std::numbers::pi_v<float>;

	[[nodiscard]] inline float WrapRotation(float degrees) noexcept
	{
		if (!std::isfinite(degrees))
			return 0.0f;

		float result = std::fmod(degrees, FullTurnDegrees);
		if (result < 0.0f)
			result += FullTurnDegrees;

		// Adding a tiny negative remainder to 360 can round back to 360.
		// This also canonicalizes negative zero to positive zero.
		if (result >= FullTurnDegrees || result == 0.0f)
			return 0.0f;
		return result;
	}

	[[nodiscard]] constexpr float ToRadians(float degrees) noexcept
	{
		return degrees * DegreesToRadiansScale;
	}

	[[nodiscard]] constexpr float ToDegrees(float radians) noexcept
	{
		return radians * RadiansToDegreesScale;
	}
} // namespace RotatorUtils

struct Rotator
{
	float pitchDegrees_ = 0.0f;
	float yawDegrees_   = 0.0f;
	float rollDegrees_  = 0.0f;

	constexpr Rotator() = default;
	Rotator(float pitchDegrees, float yawDegrees, float rollDegrees) noexcept
		: pitchDegrees_(RotatorUtils::WrapRotation(pitchDegrees)),
		  yawDegrees_(RotatorUtils::WrapRotation(yawDegrees)),
		  rollDegrees_(RotatorUtils::WrapRotation(rollDegrees)) {}

	// Public fields make editor binding simple. Call Normalize after editing
	// them directly to restore the canonical [0, 360) representation.
	Rotator& Normalize() noexcept
	{
		pitchDegrees_ = RotatorUtils::WrapRotation(pitchDegrees_);
		yawDegrees_ = RotatorUtils::WrapRotation(yawDegrees_);
		rollDegrees_ = RotatorUtils::WrapRotation(rollDegrees_);
		return *this;
	}

	[[nodiscard]] Quat ToQuat() const noexcept;
	[[nodiscard]] static Rotator FromQuat(const Quat& rotation) noexcept;
	[[nodiscard]] Vector3 InRadians() const;

	[[nodiscard]] constexpr bool operator==(const Rotator&) const = default;
};

[[nodiscard]] inline Quat Rotator::ToQuat() const noexcept
{
	// Wrap before converting so very large degree values do not lose avoidable
	// precision during the degrees-to-radians conversion.
	const float pitchRadians = RotatorUtils::ToRadians(
		RotatorUtils::WrapRotation(pitchDegrees_));
	const float yawRadians = RotatorUtils::ToRadians(
		RotatorUtils::WrapRotation(yawDegrees_));
	const float rollRadians = RotatorUtils::ToRadians(
		RotatorUtils::WrapRotation(rollDegrees_));

	const Quat roll = FromAxisAngle({ -1.0f, 0.0f, 0.0f }, rollRadians);
	const Quat pitch = FromAxisAngle({ 0.0f, -1.0f, 0.0f }, pitchRadians);
	const Quat yaw = FromAxisAngle({ 0.0f, 0.0f, 1.0f }, yawRadians);

	// Quaternion products read left-to-right in this engine. Roll first leaves
	// the forward axis fixed; pitch and yaw then produce the editor-camera
	// forward vector { cos(pitch) cos(yaw), cos(pitch) sin(yaw), sin(pitch) }.
	return Normalized(roll * pitch * yaw);
}

[[nodiscard]] inline Rotator Rotator::FromQuat(const Quat& rotation) noexcept
{
	// ToMatrix normalizes the quaternion and maps invalid input to identity.
	const Matrix matrix = ToMatrix(rotation);
	const float sinePitch = std::clamp(matrix.m[0][2], -1.0f, 1.0f);
	const float cosinePitch = std::hypot(matrix.m[0][0], matrix.m[0][1]);

	const float pitchRadians = std::atan2(sinePitch, cosinePitch);
	float yawRadians = 0.0f;
	float rollRadians = 0.0f;

	if (cosinePitch > 1e-6f)
	{
		yawRadians = std::atan2(matrix.m[0][1], matrix.m[0][0]);
		rollRadians = std::atan2(-matrix.m[1][2], matrix.m[2][2]);
	}
	else
	{
		// At +/-90 degrees of pitch, yaw and roll describe the same remaining
		// degree of freedom. Canonicalize by folding roll into yaw and setting
		// roll to zero; converting back still produces the original rotation.
		yawRadians = std::atan2(-matrix.m[1][0], matrix.m[1][1]);
	}

	return {
		RotatorUtils::ToDegrees(pitchRadians),
		RotatorUtils::ToDegrees(yawRadians),
		RotatorUtils::ToDegrees(rollRadians)
	};
}

// Euler triples are not unique, so compare their represented orientations.
[[nodiscard]] inline bool SameRotation(
	const Rotator& a,
	const Rotator& b,
	float epsilon = 1e-5f) noexcept
{
	return SameRotation(a.ToQuat(), b.ToQuat(), epsilon);
}

[[nodiscard]] inline Vector3 Rotator::InRadians() const
{
	float factor = 3.141592f / 180.f;
	return Vector3(pitchDegrees_, yawDegrees_, rollDegrees_) * factor;
}