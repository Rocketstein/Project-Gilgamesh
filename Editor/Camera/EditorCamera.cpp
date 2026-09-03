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

DirectX::XMMATRIX EditorCamera::GetViewMatrix() const noexcept
{
	using namespace DirectX;

	// Gilgamesh uses a left-handed, Z-up coordinate system:
	// +X forward, +Y right, +Z up.
	const float cosPitch = std::cos(currentState_.pitchRadians_);
	const XMVECTOR forward = XMVectorSet(
		cosPitch * std::cos(currentState_.yawRadians_),
		cosPitch * std::sin(currentState_.yawRadians_),
		std::sin(currentState_.pitchRadians_),
		0.0f);

	const XMVECTOR position = XMLoadFloat3(&currentState_.position_);
	const XMVECTOR worldUp = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);

	return XMMatrixLookToLH(position, forward, worldUp);
}

DirectX::XMMATRIX EditorCamera::GetProjectionMatrix(
	float aspectRatio) const noexcept
{
	using namespace DirectX;

	constexpr float nearPlane = 0.1f;
	constexpr float farPlane = 1000.0f;
	constexpr float orthographicHeight = 10.0f;

	const float safeAspectRatio = aspectRatio > 0.0f ? aspectRatio : 1.0f;

	if (currentState_.isOrthographic_)
	{
		return XMMatrixOrthographicLH(
			orthographicHeight * safeAspectRatio,
			orthographicHeight,
			nearPlane,
			farPlane);
	}

	return XMMatrixPerspectiveFovLH(
		currentState_.FOVRadians_,
		safeAspectRatio,
		nearPlane,
		farPlane);
}

