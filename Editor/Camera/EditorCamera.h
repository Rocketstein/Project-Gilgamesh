#pragma once
#include <DirectXMath.h>

// Minimal snapshot of the camera's current state
struct CameraState
{
	// Fixed World Position for now
	DirectX::XMFLOAT3 position_ = { -3.0f, 0.0f, 0.0f };

	float yawRadians_   = 0.0f;
	float pitchRadians_ = 0.0f;
	float FOVRadians_   = DirectX::XMConvertToRadians(60.0f);

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
	const DirectX::XMFLOAT3& GetPosition() const noexcept { return currentState_.position_; }
	void SetPosition(DirectX::XMFLOAT3 position) noexcept
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
	DirectX::XMMATRIX GetViewMatrix() const noexcept;

	[[nodiscard]]
	DirectX::XMMATRIX GetProjectionMatrix(
		float aspectRatio) const noexcept;

private:
	CameraState currentState_;
	CameraState pendingState_;

	bool isDirty_ = true;

	// More to come
};
