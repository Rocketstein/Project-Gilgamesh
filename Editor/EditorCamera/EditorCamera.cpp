#include "EditorCamera.h"

#include <cmath>

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
	constexpr Vector3 worldUp{ 0.0f, 0.0f, 1.0f };

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

