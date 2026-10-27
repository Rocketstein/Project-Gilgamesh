#include "RenderExtractionSystem.h"

#include <utility>

#include "Engine/ECS/Component/BoundsComponent.h"
#include "Engine/ECS/Component/MeshComponent.h"
#include "Engine/ECS/Component/TransformComponent.h"
#include "Engine/ECS/Framework/World.h"

namespace
{
// Append each world-space box edge once; the bounds already include the entity transform.
void AppendAABB(std::vector<DebugLine>& lines, const BoundingBox& bounds, const Vector3& color)
{
	const Vector3 corners[8]{
		{ bounds.min.x, bounds.min.y, bounds.min.z },
		{ bounds.max.x, bounds.min.y, bounds.min.z },
		{ bounds.max.x, bounds.max.y, bounds.min.z },
		{ bounds.min.x, bounds.max.y, bounds.min.z },
		{ bounds.min.x, bounds.min.y, bounds.max.z },
		{ bounds.max.x, bounds.min.y, bounds.max.z },
		{ bounds.max.x, bounds.max.y, bounds.max.z },
		{ bounds.min.x, bounds.max.y, bounds.max.z }
	};

	// The two four-edge faces and four connecting edges form the complete wire box.
	constexpr int edges[12][2]{
		{ 0, 1 }, { 1, 2 }, { 2, 3 }, { 3, 0 },
		{ 4, 5 }, { 5, 6 }, { 6, 7 }, { 7, 4 },
		{ 0, 4 }, { 1, 5 }, { 2, 6 }, { 3, 7 }
	};
	for (const auto& edge : edges)
	{
		lines.push_back({ corners[edge[0]], corners[edge[1]], color });
	}
}
} // Anonymous namespace

void RenderExtractionSystem::Update(World& world, float /*deltaTime*/)
{
	// A failed extraction must not leave an older snapshot looking like fresh output.
	DiscardFrame();

	// Build a fresh result so removing or clearing the active scene removes old draws.
	RenderFrameData frame;
	const Vector3 boundsColor{ 0.0f, 1.0f, 0.0f };
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

			// Copy diagnostics into every requested frame, even when the bounds revision is unchanged.
			if (showBounds_ && readOnlyWorld.Has<AABBComponent>(entity))
			{
				const AABBComponent& bounds = readOnlyWorld.Get<AABBComponent>(entity);
				if (bounds.valid) AppendAABB(frame.debugLines, bounds.AABB, boundsColor);
			}
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
