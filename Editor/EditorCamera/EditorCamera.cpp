#include "EditorCamera.h"

#include "Engine/Core/Logging/Logger.h"

#include <cmath>

namespace
{
	constexpr Vector3 worldUp{ 0.0f, 0.0f, 1.0f };
	constexpr Vector3 worldForward{ 1.0f, 0.0f, 0.0f };
	constexpr Vector3 worldRight{ 0.0f, 1.0f, 0.0f };
	constexpr float maxPitch = 89.9f;

	constexpr float DegToRad = std::numbers::pi_v<float> / 180.f;
} // Anonymous Namespace

EditorCamera::EditorCamera()
{	
	currentState_ = pendingState_;
}

void EditorCamera::Update()
{
	if (isDirty_)
	{
		currentState_ = pendingState_;
		isDirty_ = false;
	}
}

Matrix4 EditorCamera::GetViewMatrix() const noexcept
{
	// Gilgamesh uses a left-handed, Z-up coordinate system:
	// +X forward, +Y right, +Z up.
	Vector3 rotationRadians = currentState_.rotation_.InRadians();
	const float cosPitch = std::cos(rotationRadians.x);
	const Vector3 forward{
		cosPitch * std::cos(rotationRadians.y),
		cosPitch * std::sin(rotationRadians.y),
		std::sin(rotationRadians.x)
	};

	return LookToLH(currentState_.position_, forward, worldUp);
}

Matrix4 EditorCamera::GetProjectionMatrix(
	float aspectRatio) const noexcept
{
	constexpr float nearPlane = 0.1f;
	constexpr float farPlane = 1000.0f;
	constexpr float orthographicHeight = 10.0f;

	const float safeAspectRatio = aspectRatio > 0.0f ? aspectRatio : 1.0f;

	if (currentState_.isOrthographic_)
	{
		return OrthographicLH(
			orthographicHeight * safeAspectRatio,
			orthographicHeight,
			nearPlane,
			farPlane);
	}

	return PerspectiveFovLH(
		currentState_.FOVRadians_,
		safeAspectRatio,
		nearPlane,
		farPlane);
}

void EditorCamera::ApplyCameraIntent(const EditorCameraIntent& intent, float deltaTime)
{
	ApplyMovementIntent(intent.localMovement, deltaTime);
	ApplyLookIntent(intent.lookDelta);
	ApplyZoomIntent(intent.zoomDelta);
	ApplyToggleOrthographicIntent(intent.toggleOrthographic);
	if (isDirty_)
	{
		Update();
	}
}

void EditorCamera::ApplyMovementIntent(const Vector3& localMovement, float deltaTime)
{
	if (localMovement == Vector3{})
		return;

	Vector3 rotationRadians = currentState_.rotation_.InRadians();
	const float cosPitch = std::cos(rotationRadians.x);
	const Vector3 forward{
		cosPitch * std::cos(rotationRadians.y),
		cosPitch * std::sin(rotationRadians.y),
		std::sin(rotationRadians.x)
	};

	const Vector3 right{
		-std::sin(rotationRadians.y),
		 std::cos(rotationRadians.y),
		 0.0f
	};

	const Vector3 movementWorldSpace =
		right * localMovement.x +
		worldUp * localMovement.y +
		forward * localMovement.z;

	pendingState_.position_ += movementWorldSpace * deltaTime;
	isDirty_ = true;
}

void EditorCamera::ApplyLookIntent(const Vector2& lookDelta)
{
	if (lookDelta == Vector2{})
		return;

	Rotator& cameraRotation = pendingState_.rotation_;

	cameraRotation.yawDegrees_   += lookDelta.x * configs_.lookSensitivity;
	cameraRotation.pitchDegrees_ += lookDelta.y * configs_.lookSensitivity;

	if (cameraRotation.pitchDegrees_ > maxPitch)
		cameraRotation.pitchDegrees_ = maxPitch;
	else if (cameraRotation.pitchDegrees_ < -maxPitch)
		cameraRotation.pitchDegrees_ = -maxPitch;
	isDirty_ = true;
}

void EditorCamera::ApplyZoomIntent(float zoomDelta)
{
	if (abs(zoomDelta) < 0.0001f)
		return;

	pendingState_.FOVRadians_ -= zoomDelta * configs_.zoomSensitivity;

	if (pendingState_.FOVRadians_ < configs_.minFOV * DegToRad)
		pendingState_.FOVRadians_ = configs_.minFOV * DegToRad;
	else if (pendingState_.FOVRadians_ > configs_.maxFOV * DegToRad)
		pendingState_.FOVRadians_ = configs_.maxFOV * DegToRad;
	isDirty_ = true;
}

void EditorCamera::ApplyToggleOrthographicIntent(bool toggle)
{
	if (toggle)
	{
		pendingState_.isOrthographic_ = !pendingState_.isOrthographic_;
		isDirty_ = true;
	}
}
