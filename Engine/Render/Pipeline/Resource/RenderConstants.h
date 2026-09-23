#pragma once

#include <cstddef>

#include "Engine/Core/Math/Matrix.h"

// Constant Buffer Slot Definitions

// b0: Frame global
struct alignas(16) FrameConstants
{
	Matrix4 view;                        // 64 bytes (offset 0)
	Matrix4 projection;                  // 64 bytes (offset 64)
	Matrix4 viewProjection;              // 64 bytes (offset 128)
	Matrix4 inverseVP;                   // 64 bytes (offset 192)
	Vector3 cameraWorldPos;              // 12 bytes (offset 256)
	float cameraPadding = 0.0f;			 // 4 bytes (offset 268)
	Vector2 viewportDim;                 // 8 bytes (offset 272)
	Vector2 viewportPadding;             // 8 bytes (offset 280)
}; // Total: 288 bytes

static_assert(offsetof(FrameConstants, cameraWorldPos) == 256);
static_assert(offsetof(FrameConstants, viewportDim) == 272);
static_assert(alignof(FrameConstants) == 16);
static_assert(sizeof(FrameConstants) == 288);

// b1: Editor specific
struct alignas(16) EditorConstants
{
	// Editor Grid
	Vector4 gridColor;      // 16 bytes (offset 0)
	Vector2 gridFadeRange;  // 8 bytes (offset 16)
	float gridSpacing;      // 4 bytes (offset 24)
	float gridWidth;        // 4 bytes (offset 28)
}; // Total: 32 bytes

static_assert(alignof(EditorConstants) == 16);
static_assert(sizeof(EditorConstants) == 32);

// b2: Per-Object
struct alignas(16) ObjectConstants
{
	Matrix4 model;  // 64 bytes (offset 0)
}; // Total: 64 bytes

static_assert(alignof(ObjectConstants) == 16);
static_assert(sizeof(ObjectConstants) == 64);
