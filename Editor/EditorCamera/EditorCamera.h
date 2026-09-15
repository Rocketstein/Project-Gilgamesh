#pragma once

#include "EditorCameraConfig.h"
#include "Editor/EditorInput/EditorCameraIntent.h"
#include "Engine/Core/Math/Matrix.h"
#include "Engine/Core/Math/Rotator.h"

#include <numbers>

// Minimal snapshot of the camera's current state
struct CameraState
{
	// Fixed World Position for now
	Vector3 position_ = { -3.0f, 0.0f, 0.0f };
	Rotator rotation_ = { 0.0f, 0.0f, 0.0f };

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

	// Apply camera intent to pending state
	void ApplyCameraIntent(const EditorCameraIntent& intent, float deltaTime);

	// Accessors
	[[nodiscard]]
	const Vector3& GetPosition() const noexcept { return currentState_.position_; }
	void SetPosition(Vector3 position) noexcept
	{
		pendingState_.position_ = position;
		isDirty_ = true;
	}

	[[nodiscard]]
	const Rotator& GetRotation() const noexcept { return currentState_.rotation_; }
	void SetRotation(Rotator rotation) noexcept
	{
		pendingState_.rotation_ = rotation;
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
	Matrix4 GetViewMatrix() const noexcept;

	[[nodiscard]]
	Matrix4 GetProjectionMatrix(
		float aspectRatio) const noexcept;

	[[nodiscard]]
	const EditorCameraConfigurations& GetEditorCameraConfigurations() const { return configs_; }
	void SetEditorCameraConfigurations(EditorCameraConfigurations configs) { configs_ = configs; }

private:
	void ApplyMovementIntent(const Vector3& localMovement, float deltaTime);
	void ApplyLookIntent(const Vector2& lookDelta);
	void ApplyZoomIntent(float zoomDelta);
	void ApplyToggleOrthographicIntent(bool toggleOrthographic);

private:
	CameraState currentState_;
	CameraState pendingState_;
	EditorCameraConfigurations configs_;

	bool isDirty_ = true;

	// More to come
};
