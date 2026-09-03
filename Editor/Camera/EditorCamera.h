#pragma once
#include <DirectXMath.h>

using namespace DirectX;

// Minimal snapshot of the camera's current state
struct CameraState
{
	// Fixed World Position for now
	XMVECTOR3 position_ = XMVectorSet(-3.0f, 0.0f, 0.0f, 1.0f);

	float yawRadians_   = 0.0f;
	float pitchRadians_ = 0.0f;
	float FOVRadians_   = XMConvertToRadians(60.0f);

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
	XMVECTOR GetPosition() const { return currentState_.position_; } const;
	void SetPosition(const XMVECTOR& position) { pendingState_.position_ = position; }

	[[nodiscard]]
	float GetFOV() const { return currentState_.FOVRadians_; } const;
	void SetFOV(float fovRadians) { pendingState_.FOVRadians_ = fovRadians; }

	[[nodiscard]]
	bool IsOrthographic() const { return currentState_.isOrthographic_; }
	void SetOrthographic(bool isOrthographic) { pendingState_.isOrthographic_ = isOrthographic; }

	[[nodiscard]]
	float GetYaw() const { return currentState_.yawRadians_; } const;

	[[nodiscard]]
	float SetYaw() const { return currentState_.yawRadians_; } const;

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