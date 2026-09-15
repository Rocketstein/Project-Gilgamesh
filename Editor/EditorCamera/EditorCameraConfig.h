#pragma once

struct EditorCameraConfigurations
{
	float lookSensitivity = 0.15f;
	float zoomSensitivity = 0.1f;

	// Clamp FOV to reasonable values
	float minFOV = 15.f;
	float maxFOV = 90.f;
};