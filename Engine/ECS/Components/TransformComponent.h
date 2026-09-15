#pragma once
#include "Engine/Core/Math/Quat.h"

struct TransformComponent
{
	// Position in world space
	Vector3 position_;
	// Rotation in world space 
	Quat rotation_;
	// Scale in world space
	Vector3 scale;
};