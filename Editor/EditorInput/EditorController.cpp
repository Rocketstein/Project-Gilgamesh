#include "EditorController.h"

#include "Engine/Input/InputFrame.h"

EditorCameraIntent EditorController::Update(const InputFrame& frame)
{
	EditorCameraIntent intent;
	intent.localMovement = GetMovementIntent(frame);
	intent.lookDelta = GetLookIntent(frame);
	intent.zoomDelta = GetZoomIntent(frame);
	intent.toggleOrthographic = GetToggleOrthographicIntent(frame);
	return intent;
}

Vector3 EditorController::GetMovementIntent(const InputFrame& frame)
{
	Vector3 movement{};
	if (frame.IsDown(Key::W))
		movement.z += 1.0f;
	if (frame.IsDown(Key::S))
		movement.z -= 1.0f;
	if (frame.IsDown(Key::A))
		movement.x -= 1.0f;
	if (frame.IsDown(Key::D))
		movement.x += 1.0f;
	if (frame.IsDown(Key::Q))
		movement.y -= 1.0f;
	if (frame.IsDown(Key::E))
		movement.y += 1.0f;
	return movement * MoveSpeed;
}

Vector2 EditorController::GetLookIntent(const InputFrame& frame)
{
	return frame.GetMouseDelta() * RotateSpeed;
}

float EditorController::GetZoomIntent(const InputFrame& frame)
{
	return frame.GetWheelDelta();
}

bool EditorController::GetToggleOrthographicIntent(const InputFrame& frame)
{
	return frame.WasPressed(Key::O);
}