#pragma once
#include "EditorCameraIntent.h"

class InputFrame;

class EditorController
{
public:
	EditorCameraIntent Update(const InputFrame& frame);

private:
	Vector3 GetMovementIntent(const InputFrame& frame);
	Vector2 GetLookIntent(const InputFrame& frame);
	float GetZoomIntent(const InputFrame& frame);
	bool GetToggleOrthographicIntent(const InputFrame& frame);

private:
	float MoveSpeed = 5.0f;
	float RotateSpeed = 2.0f;

};
