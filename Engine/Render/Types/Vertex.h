#pragma once
#include "Engine/Core/Math/Vector.h"

// Prototype Vertex Types
struct SimpleVertex2D 
{
	float x, y;
	float r, g, b;
};

struct SimpleVertex3D
{
	float x, y, z;
	float r, g, b;
};
// ==========================

struct NormalVertex
{
	Vector4 color;
	Vector4 tangent;
	Vector3 position;
	Vector3 normal;
	Vector2 UV;
};