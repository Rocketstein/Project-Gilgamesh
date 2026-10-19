#pragma once

#include "Engine/Core/Extent2D.h"
#include "Engine/Core/Math/Matrix.h"

// Camera and viewport data copied for every draw submitted to one rendered view.
struct RenderView
{
	Matrix4 view = Matrix4::Identity();
	Matrix4 projection = Matrix4::Identity();
	Vector3 cameraWorldPos{}; // Camera position copied for view-dependent shaders.
	Extent2D extent{}; // Pixel dimensions used by this rendered view.
};
