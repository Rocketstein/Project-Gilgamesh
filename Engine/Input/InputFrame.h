#pragma once
#include "Engine/Core/Math/Vector.h"

struct InputFrame
{
    bool IsDown(Key) const;
    bool WasPressed(Key) const;
    bool WasReleased(Key) const;

    Vector2 mouseDelta;
    float wheelDelta;
};