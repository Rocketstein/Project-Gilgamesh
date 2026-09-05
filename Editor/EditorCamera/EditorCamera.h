#pragma once
#include "Engine/Core/Math/Matrix.h"

#include <numbers>

// Minimal snapshot of the camera's current state
struct CameraState
{
	// Fixed World Position for now
	Vector3 position_ = { -3.0f, 0.0f, 0.0f };

	float yawRadians_   = 0.7f;
	float pitchRadians_ = 0.10f;
	float FOVRadians_   = std::numbers::pi_v<float> / 3.0f;

	bool isOrthographic_ = false;
};

class EditorCamera
{
public:
	EditorCamera();
	~EditorCamera() = default;

	// Update current state from pending state if dirty
	void Update();

	// Accessors
	[[nodiscard]]
	const Vector3& GetPosition() const noexcept { return currentState_.position_; }
	void SetPosition(Vector3 position) noexcept
	{
		pendingState_.position_ = position;
		isDirty_ = true;
	}

	[[nodiscard]]
	float GetFOV() const noexcept { return currentState_.FOVRadians_; }
	void SetFOV(float fovRadians) noexcept
	{
		pendingState_.FOVRadians_ = fovRadians;
		isDirty_ = true;
	}

	[[nodiscard]]
	bool IsOrthographic() const noexcept { return currentState_.isOrthographic_; }
	void SetOrthographic(bool isOrthographic) noexcept
	{
		pendingState_.isOrthographic_ = isOrthographic;
		isDirty_ = true;
	}

	[[nodiscard]]
	float GetYaw() const noexcept { return currentState_.yawRadians_; }
	void SetYaw(float yawRadians) noexcept
	{
		pendingState_.yawRadians_ = yawRadians;
		isDirty_ = true;
	}

	[[nodiscard]]
	float GetPitch() const noexcept { return currentState_.pitchRadians_; }
	void SetPitch(float pitchRadians) noexcept
	{
		pendingState_.pitchRadians_ = pitchRadians;
		isDirty_ = true;
	}

	[[nodiscard]]
	Matrix4 GetViewMatrix() const noexcept;

	[[nodiscard]]
	Matrix4 GetProjectionMatrix(
		float aspectRatio) const noexcept;

private:
	CameraState currentState_;
	CameraState pendingState_;

	bool isDirty_ = true;

	// More to come
};
