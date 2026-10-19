#pragma once

#include "Engine/Render/Mesh/MeshHandle.h"

// Refers to shared geometry without owning or resolving its GPU resource.
struct MeshComponent
{
	MeshHandle mesh; // Invalid until geometry is explicitly assigned.
};
