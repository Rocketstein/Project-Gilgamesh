#include "PrimitiveMeshes.h"

namespace
{
	NormalVertex MakeCubeVertex(
		Vector3 position,
		Vector4 color) noexcept
	{
		// Value initialization leaves deferred tangent, normal, and UV data at zero.
		NormalVertex vertex{};
		vertex.position = position;
		vertex.color = color;
		return vertex;
	}
}

StaticMeshData PrimitiveMeshes::MakeUnitCubeMeshData()
{
	// Eight shared corners in Gilgamesh's left-handed, Z-up world space.
	constexpr float halfExtent = 0.5f;

	StaticMeshData data;
	data.vertices_ = {
		MakeCubeVertex({ -halfExtent, -halfExtent, -halfExtent }, { 1, 0, 0, 1 }),
		MakeCubeVertex({ -halfExtent,  halfExtent, -halfExtent }, { 0, 1, 0, 1 }),
		MakeCubeVertex({ -halfExtent,  halfExtent,  halfExtent }, { 0, 0, 1, 1 }),
		MakeCubeVertex({ -halfExtent, -halfExtent,  halfExtent }, { 1, 1, 0, 1 }),
		MakeCubeVertex({  halfExtent, -halfExtent, -halfExtent }, { 1, 0, 1, 1 }),
		MakeCubeVertex({  halfExtent,  halfExtent, -halfExtent }, { 0, 1, 1, 1 }),
		MakeCubeVertex({  halfExtent,  halfExtent,  halfExtent }, { 1, 1, 1, 1 }),
		MakeCubeVertex({  halfExtent, -halfExtent,  halfExtent }, { 1, 0.5f, 0, 1 }),
	};

	data.indices_ = {
		0, 2, 3,  0, 1, 2, // -X
		4, 6, 5,  4, 7, 6, // +X
		0, 7, 4,  0, 3, 7, // -Y
		1, 6, 2,  1, 5, 6, // +Y
		0, 5, 1,  0, 4, 5, // -Z
		3, 6, 7,  3, 2, 6, // +Z
	};

	return data;
}
