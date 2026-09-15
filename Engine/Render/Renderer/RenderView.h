#pragma once

#include "Engine/Core/Math/Matrix.h"

// Camera matrices shared by every draw submitted for one rendered view.
struct RenderView
{
	Matrix4 view = Matrix4::Identity();
	Matrix4 projection = Matrix4::Identity();
};
