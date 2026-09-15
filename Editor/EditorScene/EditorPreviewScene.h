#pragma once

#include <vector>

#include "Engine/Render/Mesh/MeshHandle.h"
#include "Engine/Render/Renderer/StaticMeshDrawItem.h"

class MeshManager;

// Owns the temporary content displayed in the editor preview viewport.
class EditorPreviewScene final
{
public:
	EditorPreviewScene() = default;
	~EditorPreviewScene() = default;

	EditorPreviewScene(const EditorPreviewScene&) = delete;
	EditorPreviewScene& operator=(const EditorPreviewScene&) = delete;
	EditorPreviewScene(EditorPreviewScene&&) = delete;
	EditorPreviewScene& operator=(EditorPreviewScene&&) = delete;

	// Creates the preview resources through the renderer-facing mesh manager.
	[[nodiscard]]
	bool Initialize(MeshManager& meshes);

	// Removes every preview resource owned by this scene.
	void Shutdown(MeshManager& meshes);

	// Appends the scene's currently resolvable static-mesh instances.
	void GatherDrawItems(
		const MeshManager& meshes,
		std::vector<StaticMeshDrawItem>& output) const;

private:
	// Mesh handles are meaningful only within the manager that created them.
	MeshManager* meshManager_ = nullptr;
	MeshHandle cubeMesh_;
	Matrix4 cubeTransform_ = Matrix4::Identity();
};
