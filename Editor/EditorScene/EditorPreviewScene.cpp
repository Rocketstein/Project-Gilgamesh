#include "EditorPreviewScene.h"

#include "Engine/Render/Mesh/MeshManager.h"
#include "Engine/Render/Mesh/PrimitiveMeshes.h"

bool EditorPreviewScene::Initialize(MeshManager& meshes)
{
	if (meshManager_)
	{
		if (meshManager_ != &meshes) return false;
		if (meshes.IsValid(cubeMesh_)) return true;

		// The manager discarded this resource, so the scene can rebuild it.
		meshManager_ = nullptr;
		cubeMesh_ = {};
	}

	const MeshHandle cubeMesh = meshes.CreateMesh(
		PrimitiveMeshes::MakeUnitCubeMeshData());
	if (!meshes.IsValid(cubeMesh)) return false;

	meshManager_ = &meshes;
	cubeMesh_ = cubeMesh;
	return true;
}

void EditorPreviewScene::Shutdown(MeshManager& meshes)
{
	if (meshManager_ != &meshes) return;

	if (cubeMesh_)
	{
		(void)meshes.RemoveMesh(cubeMesh_);
	}

	meshManager_ = nullptr;
	cubeMesh_ = {};
}

void EditorPreviewScene::GatherDrawItems(
	const MeshManager& meshes,
	std::vector<StaticMeshDrawItem>& output) const
{
	if (meshManager_ != &meshes) return;

	const StaticMesh* cube = meshes.TryGet(cubeMesh_);
	if (!cube) return;

	output.push_back(StaticMeshDrawItem{
		.mesh = cube,
		.localToWorld = cubeTransform_
	});
}
