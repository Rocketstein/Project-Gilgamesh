#include "UpdateBoundsSystem.h"

// Retain the declared metadata source without resolving or updating any mesh.
UpdateBoundsSystem::UpdateBoundsSystem(const MeshManager& meshes) noexcept
	: meshes_(meshes)
{
}

// API scaffold only: world bounds, validity, and revisions are intentionally left unchanged.
void UpdateBoundsSystem::Update(World& /*world*/, float /*deltaTime*/)
{
}

// Placeholder dependency check until local-bounds metadata is populated.
bool UpdateBoundsSystem::NeedsBoundsUpdate(
	const TransformComponent& /*transform*/,
	const MeshComponent& /*mesh*/,
	const AABBComponent& /*bounds*/) const noexcept
{
	return false;
}

// Placeholder bounds evaluation; no transform or bounds state is modified.
void UpdateBoundsSystem::UpdateWorldBounds(
	const TransformComponent& /*transform*/,
	const MeshComponent& /*mesh*/,
	AABBComponent& /*bounds*/) const noexcept
{
}
