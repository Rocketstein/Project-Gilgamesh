#include "RenderExtractionSystem.h"

#include <utility>

#include "Engine/ECS/Component/MeshComponent.h"
#include "Engine/ECS/Component/TransformComponent.h"
#include "Engine/ECS/Framework/World.h"

namespace
{
// Row vectors apply scale before rotation; translation occupies the final row.
Matrix4 MakeLocalToWorld(const TransformComponent& transform)
{
	Matrix basis = ToMatrix(transform.rotation_);
	for (int column = 0; column < 3; ++column)
	{
		basis.m[0][column] *= transform.scale.x;
		basis.m[1][column] *= transform.scale.y;
		basis.m[2][column] *= transform.scale.z;
	}
	return Matrix4{ basis, transform.position_ };
}
} // Anonymous namespace

void RenderExtractionSystem::Update(World& world, float /*deltaTime*/)
{
	// A failed extraction must not leave an older snapshot looking like fresh output.
	DiscardFrame();

	// Build a fresh result so removing or clearing the active scene removes old draws.
	RenderFrameData frame;
	const World& readOnlyWorld = world;
	const Scene* scene = readOnlyWorld.TryGetActiveScene();
	if (scene)
	{
		// Scene membership bounds this traversal; the World owns all component storage.
		scene->ForEachEntity([&](Entity entity)
		{
			if (!readOnlyWorld.IsAlive(entity)
				|| !readOnlyWorld.Has<TransformComponent>(entity)
				|| !readOnlyWorld.Has<MeshComponent>(entity))
			{
				return;
			}

			// Ignore unassigned geometry, leaving resource-generation validation to rendering.
			const MeshComponent& mesh = readOnlyWorld.Get<MeshComponent>(entity);
			if (!mesh.mesh.IsValid()) return;

			const TransformComponent& transform =
				readOnlyWorld.Get<TransformComponent>(entity);
			frame.staticMeshes.push_back({ mesh.mesh, MakeLocalToWorld(transform) });
		});
	}

	// Publishing replaces any result that was not consumed before the next extraction.
	pendingFrame_ = std::move(frame);
}

std::optional<RenderFrameData> RenderExtractionSystem::TakeFrame()
{
	auto frame = std::move(pendingFrame_);
	pendingFrame_.reset();
	return frame;
}

void RenderExtractionSystem::DiscardFrame() noexcept
{
	pendingFrame_.reset();
}

void RenderExtractionSystem::OnDisable()
{
	DiscardFrame();
}

void RenderExtractionSystem::OnDestroy(World& /*world*/)
{
	DiscardFrame();
}
