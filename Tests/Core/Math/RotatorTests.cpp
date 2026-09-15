#include "Engine/Core/Math/Rotator.h"
#include "Tests/TestFramework.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>
#include <random>

namespace
{
	constexpr float Pi = std::numbers::pi_v<float>;

	[[nodiscard]] bool NearlyEqualDegrees(
		float a,
		float b,
		float epsilon = 1e-4f) noexcept
	{
		const float normalizedA = RotatorUtils::ClampRotation(a);
		const float normalizedB = RotatorUtils::ClampRotation(b);
		const float difference = std::fabs(normalizedA - normalizedB);
		return std::min(difference, 360.0f - difference) <= epsilon;
	}
} // namespace

GILGAMESH_TEST("Core.Math.Rotator", "DegreeNormalization")
{
	const Rotator wrapped{ -90.0f, 450.0f, 720.0f };
	GILGAMESH_CHECK_MESSAGE(
		wrapped.pitchDegrees_ == 270.0f
			&& wrapped.yawDegrees_ == 90.0f
			&& wrapped.rollDegrees_ == 0.0f,
		"construction must wrap every angle into [0, 360)");

	Rotator edited;
	edited.pitchDegrees_ = 1080.0f;
	edited.yawDegrees_ = -450.0f;
	edited.rollDegrees_ = 361.0f;
	edited.Normalize();
	GILGAMESH_CHECK_MESSAGE(
		edited.pitchDegrees_ == 0.0f
			&& edited.yawDegrees_ == 270.0f
			&& edited.rollDegrees_ == 1.0f,
		"Normalize must restore canonical directly edited fields");

	const Rotator invalid{
		std::numeric_limits<float>::infinity(),
		-std::numeric_limits<float>::infinity(),
		std::numeric_limits<float>::quiet_NaN()
	};
	GILGAMESH_CHECK_MESSAGE(invalid == Rotator{},
		"non-finite angles must safely become the neutral rotation");

	const float huge = RotatorUtils::ClampRotation(std::numeric_limits<float>::max());
	GILGAMESH_CHECK_MESSAGE(huge >= 0.0f && huge < 360.0f,
		"large finite angles must normalize without repeated subtraction");
	GILGAMESH_CHECK_MESSAGE(!std::signbit(RotatorUtils::ClampRotation(-360.0f)),
		"normalization must collapse negative zero");
}

GILGAMESH_TEST("Core.Math.Rotator", "AxisAndCameraConventions")
{
	const Vector3 forward{ 1.0f, 0.0f, 0.0f };
	const Vector3 right{ 0.0f, 1.0f, 0.0f };

	GILGAMESH_CHECK_MESSAGE(
		NearlyEquals(forward * Rotator{ 0.0f, 90.0f, 0.0f }.ToQuat(), right),
		"positive yaw must turn forward toward the right around +Z");
	GILGAMESH_CHECK_MESSAGE(
		NearlyEquals(
			forward * Rotator{ 90.0f, 0.0f, 0.0f }.ToQuat(),
			{ 0.0f, 0.0f, 1.0f }),
		"positive pitch must turn forward upward around -Y");
	GILGAMESH_CHECK_MESSAGE(
		NearlyEquals(
			right * Rotator{ 0.0f, 0.0f, 90.0f }.ToQuat(),
			{ 0.0f, 0.0f, -1.0f }),
		"positive roll must bank right around -X");

	const Rotator rotation{ 30.0f, 45.0f, 20.0f };
	const float pitch = 30.0f * Pi / 180.0f;
	const float yaw = 45.0f * Pi / 180.0f;
	const Vector3 expectedForward{
		std::cos(pitch) * std::cos(yaw),
		std::cos(pitch) * std::sin(yaw),
		std::sin(pitch)
	};
	GILGAMESH_CHECK_MESSAGE(
		NearlyEquals(forward * rotation.ToQuat(), expectedForward),
		"rotator forward direction must match the editor-camera convention");
}

GILGAMESH_TEST("Core.Math.Rotator", "CompositionOrder")
{
	const Rotator rotation{ 25.0f, 70.0f, 15.0f };
	const Quat roll = FromAxisAngle(
		{ -1.0f, 0.0f, 0.0f }, RotatorUtils::ToRadians(15.0f));
	const Quat pitch = FromAxisAngle(
		{ 0.0f, -1.0f, 0.0f }, RotatorUtils::ToRadians(25.0f));
	const Quat yaw = FromAxisAngle(
		{ 0.0f, 0.0f, 1.0f }, RotatorUtils::ToRadians(70.0f));

	GILGAMESH_CHECK_MESSAGE(SameRotation(rotation.ToQuat(), roll * pitch * yaw),
		"ToQuat must apply roll, then pitch, then yaw");
}

GILGAMESH_TEST("Core.Math.Rotator", "QuaternionRoundTrips")
{
	const Rotator ordinary{ 30.0f, 45.0f, 15.0f };
	const Rotator recovered = Rotator::FromQuat(ordinary.ToQuat());
	GILGAMESH_CHECK_MESSAGE(
		NearlyEqualDegrees(recovered.pitchDegrees_, ordinary.pitchDegrees_)
			&& NearlyEqualDegrees(recovered.yawDegrees_, ordinary.yawDegrees_)
			&& NearlyEqualDegrees(recovered.rollDegrees_, ordinary.rollDegrees_),
		"a non-singular rotation must preserve its canonical Euler angles");

	// Euler angles beyond +/-90 degrees of pitch have an equivalent, canonical
	// representation. FromQuat deliberately selects that representation.
	const Rotator alternate = Rotator::FromQuat(
		Rotator{ 120.0f, 20.0f, 30.0f }.ToQuat());
	GILGAMESH_CHECK_MESSAGE(
		NearlyEqualDegrees(alternate.pitchDegrees_, 60.0f)
			&& NearlyEqualDegrees(alternate.yawDegrees_, 200.0f)
			&& NearlyEqualDegrees(alternate.rollDegrees_, 210.0f),
		"FromQuat must choose a deterministic equivalent Euler representation");

	const Rotator positiveLock = Rotator::FromQuat(
		Rotator{ 90.0f, 40.0f, 20.0f }.ToQuat());
	const Rotator negativeLock = Rotator::FromQuat(
		Rotator{ 270.0f, 40.0f, 20.0f }.ToQuat());
	GILGAMESH_CHECK_MESSAGE(
		NearlyEqualDegrees(positiveLock.pitchDegrees_, 90.0f, 1e-3f)
			&& NearlyEqualDegrees(positiveLock.yawDegrees_, 20.0f, 1e-3f)
			&& NearlyEqualDegrees(positiveLock.rollDegrees_, 0.0f, 1e-3f)
			&& NearlyEqualDegrees(negativeLock.pitchDegrees_, 270.0f, 1e-3f)
			&& NearlyEqualDegrees(negativeLock.yawDegrees_, 60.0f, 1e-3f)
			&& NearlyEqualDegrees(negativeLock.rollDegrees_, 0.0f, 1e-3f),
		"gimbal lock must fold roll into yaw and return zero roll");

	const Rotator cases[] = {
		{},
		{ 30.0f, 45.0f, 15.0f },
		{ 330.0f, 270.0f, 25.0f },
		{ 180.0f, 35.0f, 15.0f },
		{ 90.0f, 35.0f, 20.0f },
		{ 270.0f, 35.0f, 20.0f }
	};

	for (const Rotator& original : cases)
	{
		const Rotator roundTrip = Rotator::FromQuat(original.ToQuat());
		GILGAMESH_CHECK_MESSAGE(SameRotation(roundTrip, original, 2e-5f),
			"quaternion conversion must preserve ordinary and gimbal-lock rotations");
		GILGAMESH_CHECK_MESSAGE(
			roundTrip.pitchDegrees_ >= 0.0f && roundTrip.pitchDegrees_ < 360.0f
				&& roundTrip.yawDegrees_ >= 0.0f && roundTrip.yawDegrees_ < 360.0f
				&& roundTrip.rollDegrees_ >= 0.0f && roundTrip.rollDegrees_ < 360.0f,
			"FromQuat must return canonical degree fields");
	}

	const Quat source = ordinary.ToQuat();
	GILGAMESH_CHECK_MESSAGE(
		SameRotation(Rotator::FromQuat(-source).ToQuat(), source)
			&& SameRotation(Rotator::FromQuat(source * 1e20f).ToQuat(), source)
			&& SameRotation(Rotator::FromQuat(source * 1e-20f).ToQuat(), source),
		"FromQuat must accept equivalent signed and scaled quaternion values");
	GILGAMESH_CHECK_MESSAGE(
		Rotator::FromQuat(Quat{ 0.0f, 0.0f, 0.0f, 0.0f }) == Rotator{}
			&& Rotator::FromQuat(Quat{
				std::numeric_limits<float>::quiet_NaN(), 0.0f, 0.0f, 1.0f }) == Rotator{}
			&& Rotator::FromQuat(Quat{
				std::numeric_limits<float>::infinity(), 0.0f, 0.0f, 1.0f }) == Rotator{},
		"degenerate and non-finite quaternions must convert to the neutral rotator");
}

GILGAMESH_TEST("Core.Math.Rotator", "RandomRoundTrips")
{
	std::mt19937 random{ 0x524F54u };
	std::uniform_real_distribution<float> degrees{ -720.0f, 720.0f };

	for (int iteration = 0; iteration < 1000; ++iteration)
	{
		const Rotator original{ degrees(random), degrees(random), degrees(random) };
		const Rotator recovered = Rotator::FromQuat(original.ToQuat());
		GILGAMESH_CHECK_MESSAGE(SameRotation(recovered, original, 5e-5f),
			"random Rotator-Quat round trips must preserve orientation");
	}
}
