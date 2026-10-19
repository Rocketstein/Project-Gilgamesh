#pragma once

#include <cstdint>
#include <vector>

#include "Engine/Core/Math/Matrix.h"
#include "Engine/Render/Mesh/MeshHandle.h"

// One scene instance copied without retaining references to ECS storage.
struct ExtractedStaticMesh
{
	MeshHandle mesh; // Shared geometry identity; resource lifetime is managed separately.
	Matrix4 localToWorld = Matrix4::Identity(); // Evaluated world transform.
};

// Owns the camera-independent input for one render submission.
struct RenderFrameData
{
	std::uint64_t frameNumber = 0; // Assigned by the application when submitting the frame.
	std::vector<ExtractedStaticMesh> staticMeshes; // Copied instances shared by all views.
};
