#include "Engine/Render/Mesh/PrimitiveMeshes.h"
#include "Tests/TestFramework.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

GILGAMESH_TEST("Engine.Render.PrimitiveMeshes", "BuildsUnitCube")
{
	const StaticMeshData cube =
		PrimitiveMeshes::MakeUnitCubeMeshData();

	GILGAMESH_CHECK_MESSAGE(cube.vertices_.size() == 8,
		"the shared-corner cube must contain eight vertices");
	GILGAMESH_CHECK_MESSAGE(cube.indices_.size() == 36,
		"the cube must contain twelve indexed triangles");

	bool indicesAreValid = true;
	for (const std::uint32_t index : cube.indices_)
	{
		if (index >= cube.vertices_.size())
		{
			indicesAreValid = false;
			break;
		}
	}

	GILGAMESH_CHECK_MESSAGE(indicesAreValid,
		"every cube index must reference an existing vertex");
}

GILGAMESH_TEST("Engine.Render.PrimitiveMeshes", "BuildsEveryCubeCorner")
{
	const StaticMeshData cube =
		PrimitiveMeshes::MakeUnitCubeMeshData();

	std::array<bool, 8> corners{};
	bool positionsAreValid = true;
	bool deferredAttributesAreZero = true;

	for (const NormalVertex& vertex : cube.vertices_)
	{
		const bool validX = std::fabs(vertex.position.x) == 0.5f;
		const bool validY = std::fabs(vertex.position.y) == 0.5f;
		const bool validZ = std::fabs(vertex.position.z) == 0.5f;
		positionsAreValid &= validX && validY && validZ;

		if (validX && validY && validZ)
		{
			const std::size_t corner =
				(vertex.position.x > 0.0f ? 4u : 0u) |
				(vertex.position.y > 0.0f ? 2u : 0u) |
				(vertex.position.z > 0.0f ? 1u : 0u);
			corners[corner] = true;
		}

		deferredAttributesAreZero &=
			NearlyEquals(vertex.tangent, Vector4{}) &&
			NearlyEquals(vertex.normal, Vector3{}) &&
			NearlyEquals(vertex.UV, Vector2{});
	}

	bool containsEveryCorner = true;
	for (const bool cornerExists : corners)
	{
		containsEveryCorner &= cornerExists;
	}

	GILGAMESH_CHECK_MESSAGE(positionsAreValid && containsEveryCorner,
		"the cube must contain each combination of plus or minus half extent");
	GILGAMESH_CHECK_MESSAGE(deferredAttributesAreZero,
		"tangent, normal, and UV data must remain zero until those passes are added");
}
