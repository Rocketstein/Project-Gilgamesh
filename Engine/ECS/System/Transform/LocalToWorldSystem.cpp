#include "LocalToWorldSystem.h"

#include <atomic>
#include <cstdint>

#include "TransformHierarchySystem.h"
#include "Engine/Core/Logging/Logger.h"
#include "Engine/ECS/Component/TransformComponent.h"
#include "Engine/ECS/Framework/World.h"

namespace
{
// Keep publication revisions distinct even when a component or system is recreated.
std::uint64_t NextWorldRevision() noexcept
{
	static std::atomic<std::uint64_t> nextRevision{ 1 };
	return nextRevision.fetch_add(1, std::memory_order_relaxed);
}
} // Anonymous namespace

// Borrow the topology used to evaluate parents before their descendants.
LocalToWorldSystem::LocalToWorldSystem(const TransformHierarchySystem& hierarchy) noexcept
	: hierarchy_(hierarchy)
{
}

// Propagate changed inputs once through the hierarchy's validated parent-first order.
void LocalToWorldSystem::Update(World& world, float /*deltaTime*/)
{
	bool skippedUninitializedInput = false;
	for (Entity entity : hierarchy_.GetTraversalOrder())
	{
		TransformComponent& transform = world.Get<TransformComponent>(entity);

		// Raw authored links may contain errors repaired only in the hierarchy cache.
		const Entity parent = hierarchy_.GetParent(entity);
		const TransformComponent* parentTransform = parent.IsNull()
			? nullptr
			: &world.Get<TransformComponent>(parent);

		// Do not publish default matrices as evaluated results when a producer has not run.
		if (transform.localRevision == 0 ||
			(parentTransform && parentTransform->worldRevision == 0))
		{
			skippedUninitializedInput = true;
			continue;
		}

		if (NeedsWorldUpdate(transform, parent, parentTransform))
		{
			UpdateWorldMatrix(transform, parent, parentTransform);
		}
	}

	// Report a scheduling problem once per update rather than once per descendant.
	if (skippedUninitializedInput)
	{
		GILGAMESH_LOG(Runtime, Warning,
			"Skipped uninitialized transform inputs; run hierarchy and local evaluation before world evaluation.");
	}
}

// Compare the current inputs with those consumed by the last successful evaluation.
bool LocalToWorldSystem::NeedsWorldUpdate(
	const TransformComponent& transform,
	Entity parent,
	const TransformComponent* parentTransform) noexcept
{
	const std::uint64_t parentWorldRevision =
		parentTransform ? parentTransform->worldRevision : 0;

	return transform.worldRevision == 0
		|| transform.evaluatedLocalRevision != transform.localRevision
		|| transform.evaluatedParent != parent
		|| transform.evaluatedParentWorldRevision != parentWorldRevision;
}

// Publish changed world matrices while consuming every successfully evaluated input.
void LocalToWorldSystem::UpdateWorldMatrix(
	TransformComponent& transform,
	Entity parent,
	const TransformComponent* parentTransform) noexcept
{
	// Keep the full affine product, including shear from nonuniform scale and rotation.
	const Matrix4 candidate = parentTransform
		? transform.localToParent * parentTransform->localToWorld
		: transform.localToParent;

	// An input change may produce the same matrix; only actual output changes propagate.
	if (transform.worldRevision == 0 || transform.localToWorld != candidate)
	{
		transform.localToWorld = candidate;
		transform.worldRevision = NextWorldRevision();
	}

	// Remember equal-result parent changes as well, so they are not evaluated every frame.
	transform.evaluatedLocalRevision = transform.localRevision;
	transform.evaluatedParent = parent;
	transform.evaluatedParentWorldRevision =
		parentTransform ? parentTransform->worldRevision : 0;
}
