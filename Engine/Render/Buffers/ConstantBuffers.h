#pragma once
#include <DirectXMath.h>

struct alignas(16) ObjectConstants
{
    DirectX::XMFLOAT4X4 modelViewProjection;
};