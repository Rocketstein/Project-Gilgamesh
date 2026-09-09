#include "EditorCamera.h"

#include "Engine/Core/Logging/Logger.h"

#include <cmath>

namespace
{
	constexpr Vector3 worldUp{ 0.0f, 0.0f, 1.0f };
	constexpr Vector3 worldForward{ 1.0f, 0.0f, 0.0f };
	constexpr Vector3 worldRight{ 0.0f, 1.0f, 0.0f };

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
	const float cosPitch = std::cos(currentState_.pitchRadians_);
	const Vector3 forward{
		cosPitch * std::cos(currentState_.yawRadians_),
		cosPitch * std::sin(currentState_.yawRadians_),
		std::sin(currentState_.pitchRadians_)
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
}

void EditorCamera::ApplyMovementIntent(const Vector3& localMovement, float deltaTime)
{
	if (localMovement == Vector3{})
		return;

	GILGAMESH_LOG(Core, Trace, "Applying movement intent: localMovement");

	const float cosPitch = std::cos(currentState_.pitchRadians_);
	const Vector3 forward{
		cosPitch * std::cos(currentState_.yawRadians_),
		cosPitch * std::sin(currentState_.yawRadians_),
		std::sin(currentState_.pitchRadians_)
	};

	const Vector3 right = Cross(worldUp, forward);
	Vector3 movementWorldSpace =
		forward * localMovement.z +
		right * localMovement.y +
		worldUp * localMovement.x;

	pendingState_.position_ += movementWorldSpace * deltaTime;
	isDirty_ = true;
}

void EditorCamera::ApplyLookIntent(const Vector2& lookDelta)
{
	if (lookDelta == Vector2{})
		return;

	constexpr float sensitivity = 0.002f;
	pendingState_.yawRadians_ += lookDelta.x * sensitivity;
	pendingState_.pitchRadians_ += lookDelta.y * sensitivity;

	// Clamp pitch to avoid gimbal lock
	constexpr float maxPitch = std::numbers::pi_v<float> / 2.0f - 0.01f;
	if (pendingState_.pitchRadians_ > maxPitch)
		pendingState_.pitchRadians_ = maxPitch;
	else if (pendingState_.pitchRadians_ < -maxPitch)
		pendingState_.pitchRadians_ = -maxPitch;
	isDirty_ = true;
}

void EditorCamera::ApplyZoomIntent(float zoomDelta)
{
	if (zoomDelta < 0.0001f)
		return;
	constexpr float zoomSensitivity = 0.1f;
	pendingState_.FOVRadians_ -= zoomDelta * zoomSensitivity;

	// Clamp FOV to reasonable values
	constexpr float minFOV = std::numbers::pi_v<float> / 12.0f; // 15 degrees
	constexpr float maxFOV = std::numbers::pi_v<float> / 2.0f; // 90 degrees

	if (pendingState_.FOVRadians_ < minFOV)
		pendingState_.FOVRadians_ = minFOV;
	else if (pendingState_.FOVRadians_ > maxFOV)
		pendingState_.FOVRadians_ = maxFOV;
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
