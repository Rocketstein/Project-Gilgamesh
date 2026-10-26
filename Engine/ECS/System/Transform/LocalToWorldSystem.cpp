#include "LocalToWorldSystem.h"

// Retain the declared dependency without performing any transform evaluation.
LocalToWorldSystem::LocalToWorldSystem(const TransformHierarchySystem& hierarchy) noexcept
	: hierarchy_(hierarchy)
{
}

// API scaffold only: world matrices and revisions are intentionally left unchanged.
void LocalToWorldSystem::Update(World& /*world*/, float /*deltaTime*/)
{
}

// Placeholder dependency check until revision-based propagation is implemented.
bool LocalToWorldSystem::NeedsWorldUpdate(
	const TransformComponent& /*transform*/,
	Entity /*parent*/,
	const TransformComponent* /*parentTransform*/) noexcept
{
	return false;
}

// Placeholder propagation; no component state is modified.
void LocalToWorldSystem::UpdateWorldMatrix(
	TransformComponent& /*transform*/,
	Entity /*parent*/,
	const TransformComponent* /*parentTransform*/) noexcept
{
}
