#pragma once
#include "Engine/Core/Math/Matrix.h"

// Constant Buffer Slot Definitions

// b0: Frame global
struct FrameConstants
{
	Matrix4 view;
	Matrix4 projection;
	Matrix4 viewProjection;
	Matrix4 inverseVP;
	Vector3 cameraWorldPos;
	Vector2 viewportDim;
};

// b1: Editor specific
struct EditorConstants
{
	// Editor Grid
	Vector4 gridColor;
	Vector2 gridFadeRange;
	float   gridSpacing;
	float   gridWidth;
};

// b2: Per-Object
struct ObjectConstants
{
	Matrix4 model;
};