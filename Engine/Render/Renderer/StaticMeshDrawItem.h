#pragma once

#include "Engine/Core/Math/Matrix.h"

class StaticMesh;

// Frame-local, non-owning description of one static-mesh draw.
struct StaticMeshDrawItem
{
	const StaticMesh* mesh = nullptr;
	Matrix4 localToWorld = Matrix4::Identity();
};
