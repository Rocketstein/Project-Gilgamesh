#include "RenderExtractionSystem.h"

#include <utility>

#include "Engine/ECS/Component/MeshComponent.h"
#include "Engine/ECS/Component/TransformComponent.h"
#include "Engine/ECS/Framework/World.h"

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

			// Extract only a completed world-transform evaluation.
			if (transform.worldRevision == 0) return;

			frame.staticMeshes.push_back({
				mesh.mesh,
				transform.localToWorld
				});
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
