#pragma once

#include <string>

#include "Engine/ECS/Framework/EntityRegistry.h"
#include "Engine/ECS/Framework/Scene.h"

// Owns ECS storage and coordinates entity lifetimes with scene membership.
class World final
{
public:
	World() = default;

	// Create a scene, generating a name when none is supplied.
	[[nodiscard]] SceneHandle CreateScene(std::string name = "");
	// Destroy the scene's entities and invalidate its handle before slot reuse.
	bool DestroyScene(SceneHandle handle);
	[[nodiscard]] const Scene* TryGetScene(SceneHandle handle) const noexcept;
	[[nodiscard]] bool IsSceneHandleValid(SceneHandle handle) const noexcept;

	// Create an entity owned directly by the world, without scene membership.
	[[nodiscard]]
	Entity CreateEntity();

	[[nodiscard]]
	Entity ReserveEntity();

	void CancelEntityReservation(Entity entity);

	void CommitEntity(Entity entity, SceneHandle handle);

	// Register an entity in a scene, or return a null entity for an invalid handle.
	[[nodiscard]]
	Entity CreateEntity(SceneHandle handle);

	// Destroy a live entity and remove its scene membership; stale handles are ignored.
	void DestroyEntity(Entity entity);

	[[nodiscard]]
	bool IsAlive(Entity entity) const noexcept;

	// Component access delegates to the registry; structural changes can invalidate references.
	template <typename Component, typename... Args>
	Component& Emplace(Entity entity, Args&&... args)
	{
		return entityRegistry_.Emplace<Component>(entity, std::forward<Args>(args)...);
	}

	template <typename Component>
	[[nodiscard]] bool Has(Entity entity) const
	{
		return entityRegistry_.Has<Component>(entity);
	}

	template <typename Component>
	[[nodiscard]] Component& Get(Entity entity)
	{
		return entityRegistry_.Get<Component>(entity);
	}

	template <typename Component>
	[[nodiscard]] const Component& Get(Entity entity) const
	{
		return entityRegistry_.Get<Component>(entity);
	}

	// Visit matching components across the whole world, including entities without a scene.
	// Keep structural changes outside the traversal to preserve chunk rows and references.
	template <typename... Components, typename Fn>
	void ForEach(Fn&& fn)
	{
		entityRegistry_.ForEach<Components...>(std::forward<Fn>(fn));
	}


private:
	friend class ECSCommandBuffer;

	// Owns entity handles, component storage, and archetype queries.
	EntityRegistry entityRegistry_;
	// Reverse scene membership; entities without a scene have no entry.
	std::map<Entity, SceneHandle> entityScenes_;

	// =============================================================================
	// Scene Management
	// =============================================================================
	// A scene slot retains its generation while empty, allowing stale handles to be rejected.
	struct SceneItem
	{
		// Empty after destruction until this slot is reused.
		std::unique_ptr<Scene> scene;
		// Advanced on destruction so old handles cannot address a reused slot.
		std::uint32_t generation = 0;
	};
	// Slots keep their indices across reuse and are addressed by SceneHandle::id.
	std::vector<SceneItem> scenes_;
	//std::vector<std::uint32_t> activeScenes_;
	// Vacant slots available for reuse; exhausted generations are excluded.
	std::vector<std::uint32_t> freeSceneIndices_;
};
