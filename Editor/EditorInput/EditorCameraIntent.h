#pragma once
#include "Engine/Core/Math/Vector.h"

struct EditorCameraIntent
{
    Vector3 localMovement;
    Vector2 lookDelta;
    float zoomDelta = 0.0f;
    bool toggleOrthographic = false;
};