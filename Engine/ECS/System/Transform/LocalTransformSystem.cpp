#include "LocalTransformSystem.h"

#include "Engine/ECS/Component/TransformComponent.h"
#include "Engine/ECS/Framework/QueryTerm.h"
#include "Engine/ECS/Framework/World.h"

// Recompose only selected chunks, then consume the writes produced by this complete pass.
void LocalTransformSystem::Update(World& world, float /*deltaTime*/)
{
	world.ForEachComponent<QueryTerm<TransformComponent, QueryFlags::ReadWrite | QueryFlags::Changed>>(
		lastObservedVersion_,
		[](TransformComponent& transform)
		{
			UpdateLocalMatrix(transform);
		});

	// This callback writes only its own caches; a failed pass leaves the previous cursor intact.
	lastObservedVersion_ = world.GetChangeVersion();
}

// Re-enabling must reconsider current source values, including writes made while disabled.
void LocalTransformSystem::OnEnable()
{
	lastObservedVersion_ = 0;
}

// Disabling invalidates this consumer's observation, not any other system's change state.
void LocalTransformSystem::OnDisable()
{
	lastObservedVersion_ = 0;
}

// A reused system lifecycle starts without a cursor into its former world's storage.
void LocalTransformSystem::OnDestroy(World& /*world*/)
{
	lastObservedVersion_ = 0;
}

// Publish only effective local-matrix changes, even when a coarse chunk write selected this row.
void LocalTransformSystem::UpdateLocalMatrix(TransformComponent& transform) noexcept
{
	// Build the scale and rotation matrices.
	const Matrix scaleMatrix{
		transform.scale.x, 0.0f,              0.0f,
		0.0f,              transform.scale.y, 0.0f,
		0.0f,              0.0f,              transform.scale.z
	};

	const Matrix rotationMatrix = ToMatrix(transform.rotation_);

	// Append translation using the engine's affine matrix constructor.
	const Matrix4 candidate{
		scaleMatrix * rotationMatrix,
		transform.position_
	};

	// The first evaluation must publish even when the matrix is already identity.
	if (transform.localRevision != 0 &&
		transform.localToParent == candidate)
	{
		return;
	}

	// Update only the state owned by local transform evaluation.
	transform.localToParent = candidate;
	++transform.localRevision;
}
