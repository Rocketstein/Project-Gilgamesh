#include "LocalTransformSystem.h"

#include "Engine/ECS/Component/TransformComponent.h"
#include "Engine/ECS/Framework/World.h"

void LocalTransformSystem::Update(World& world, float /*deltaTime*/)
{
	world.ForEachComponent<TransformComponent>(
		[](TransformComponent& transform)
		{
			UpdateLocalMatrix(transform);
		});
}

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
