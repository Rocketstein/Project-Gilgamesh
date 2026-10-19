#pragma once

#include "Engine/Core/Math/Quat.h"

// World-space transform until hierarchical transform evaluation is introduced.
struct TransformComponent
{
	Vector3 position_{}; // Position in world space.
	Quat rotation_ = Quat::Identity(); // Orientation in world space.
	Vector3 scale{ 1.0f, 1.0f, 1.0f }; // Unit scale keeps a default mesh visible.
};
