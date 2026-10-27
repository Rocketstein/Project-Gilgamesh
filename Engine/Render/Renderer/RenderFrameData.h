#pragma once

#include <cstdint>
#include <vector>

#include "Engine/Core/Math/Matrix.h"
#include "Engine/Core/Math/Vector.h"
#include "Engine/Render/Mesh/MeshHandle.h"

// One scene instance copied without retaining references to ECS storage.
struct ExtractedStaticMesh
{
	MeshHandle mesh; // Shared geometry identity; resource lifetime is managed separately.
	Matrix4 localToWorld = Matrix4::Identity(); // Evaluated world transform.
};

// One world-space diagnostic segment copied independently of ECS storage and camera selection.
struct DebugLine
{
	Vector3 start{}; // First endpoint in world space.
	Vector3 end{}; // Second endpoint in world space.
	Vector3 color{ 1.0f, 1.0f, 1.0f }; // RGB shared by both endpoints.
};

// Owns the camera-independent input for one render submission.
struct RenderFrameData
{
	std::uint64_t frameNumber = 0; // Assigned by the application when submitting the frame.
	std::vector<ExtractedStaticMesh> staticMeshes; // Copied instances shared by all views.
	std::vector<DebugLine> debugLines; // Owned world-space diagnostic segments shared by all views.
};
