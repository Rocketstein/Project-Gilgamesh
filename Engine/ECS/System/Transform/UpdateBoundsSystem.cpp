#include "UpdateBoundsSystem.h"

#include <atomic>
#include <cstdint>

#include "Engine/ECS/Component/BoundsComponent.h"
#include "Engine/ECS/Component/MeshComponent.h"
#include "Engine/ECS/Component/TransformComponent.h"
#include "Engine/ECS/Framework/World.h"
#include "Engine/Render/Mesh/MeshManager.h"

namespace
{
// Keep spatial consumers from confusing recreated components with earlier publications.
std::uint64_t NextBoundsRevision() noexcept
{
	static std::atomic<std::uint64_t> nextRevision{ 1 };
	return nextRevision.fetch_add(1, std::memory_order_relaxed);
}
} // Anonymous namespace

// Borrow shared CPU-local bounds without retaining mesh or GPU resource pointers.
UpdateBoundsSystem::UpdateBoundsSystem(const MeshManager& meshes) noexcept
	: meshes_(meshes)
{
}

// Update existing per-entity bounds after world matrices have been evaluated.
void UpdateBoundsSystem::Update(World& world, float /*deltaTime*/)
{
	world.ForEachComponent<TransformComponent, MeshComponent, AABBComponent>(
		[this](const TransformComponent& transform, const MeshComponent& mesh, AABBComponent& bounds)
		{
			// Each consumer tracks its own inputs without clearing transform revisions.
			if (NeedsBoundsUpdate(transform, mesh, bounds))
			{
				UpdateWorldBounds(transform, mesh, bounds);
			}
		});
}

// Observe resource availability as well as authored mesh and evaluated transform changes.
bool UpdateBoundsSystem::NeedsBoundsUpdate(
	const TransformComponent& transform,
	const MeshComponent& mesh,
	const AABBComponent& bounds) const noexcept
{
	BoundingBox localBounds{};
	std::uint64_t meshBoundsRevision = 0;
	if (!meshes_.TryGetLocalBounds(mesh.mesh, localBounds, meshBoundsRevision))
	{
		// A removed resource can leave an unchanged, now-stale handle in the component.
		meshBoundsRevision = 0;
	}

	return bounds.revision == 0
		|| bounds.sourceWorldRevision != transform.worldRevision
		|| bounds.sourceMesh != mesh.mesh
		|| bounds.sourceMeshBoundsRevision != meshBoundsRevision;
}

// Publish meaningful output changes and acknowledge every evaluated source input.
void UpdateBoundsSystem::UpdateWorldBounds(
	const TransformComponent& transform,
	const MeshComponent& mesh,
	AABBComponent& bounds) const noexcept
{
	// Start from original mesh-local bounds on every evaluation.
	BoundingBox localBounds{};
	std::uint64_t meshBoundsRevision = 0;
	const bool hasLocalBounds = meshes_.TryGetLocalBounds(mesh.mesh, localBounds, meshBoundsRevision);
	if (!hasLocalBounds) meshBoundsRevision = 0;

	BoundingBox candidate{};
	bool candidateValid = false;
	if (hasLocalBounds && transform.worldRevision != 0)
	{
		candidate = TransformBounds(localBounds, transform.localToWorld);
		candidateValid = candidate.IsValid();
	}

	// Invalid coordinates are not meaningful; repeated invalid results remain stable.
	const bool changed = bounds.revision == 0
		|| bounds.valid != candidateValid
		|| (candidateValid && (bounds.AABB.min != candidate.min || bounds.AABB.max != candidate.max));
	if (candidateValid) bounds.AABB = candidate;
	bounds.valid = candidateValid;
	if (changed) bounds.revision = NextBoundsRevision();

	// Equal-result input changes must not trigger the same work again on the next frame.
	bounds.sourceWorldRevision = transform.worldRevision;
	bounds.sourceMesh = mesh.mesh;
	bounds.sourceMeshBoundsRevision = meshBoundsRevision;
}
