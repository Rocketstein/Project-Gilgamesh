#pragma once

#include <string>

#include "ECSCommandBuffer.h"
#include "EntityRegistry.h"
#include "Scene.h"

// Owns ECS storage and coordinates entity lifetimes with scene membership.
class World final
{
public:
	World();

	void Update(float deltaTime);

	// Create a scene, generating a name when none is supplied.
	[[nodiscard]] SceneHandle CreateScene(std::string name = "");
	// Destroy the scene's entities and invalidate its handle before slot reuse.
	bool DestroyScene(SceneHandle handle);
	[[nodiscard]] const Scene* TryGetScene(SceneHandle handle) const noexcept;
	[[nodiscard]] bool IsSceneHandleValid(SceneHandle handle) const noexcept;

	[[nodiscard]]
	bool IsAlive(Entity entity) const noexcept;

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

	std::uint32_t GetWorldID() const noexcept { return worldID_; }

public:
	// Public Buffer request API
	Entity CreateEntity();

	Entity CreateEntity(SceneHandle handle);

	void RemoveEntity(Entity entity);

	template <typename Component>
	void RemoveComponent(Entity entity)
	{
		commandBuffer_.RemoveComponent<Component>(entity);
	}

	template <typename Component, typename... Args>
	void Emplace(Entity entity, Args&&... args)
	{
		commandBuffer_.EmplaceComponent<Component>(
			entity, std::forward<Args>(args)...);
	}

private:
	// Direct execution functions. Outside sources other than ECSCommandBuffer should use buffer request instead.
	// Component access delegates to the registry; structural changes can invalidate references.
	template <typename Component, typename... Args>
	Component& EmplaceNow(Entity entity, Args&&... args)
	{
		return entityRegistry_.Emplace<Component>(entity, std::forward<Args>(args)...);
	}

	// Register an entity in a scene, or return a null entity for an invalid handle.
	[[nodiscard]]
	Entity CreateEntityNow(SceneHandle handle);

	// Create an entity owned directly by the world, without scene membership.
	[[nodiscard]]
	Entity CreateEntityNow();

	// Destroy a live entity and remove its scene membership; stale handles are ignored.
	void DestroyEntityNow(Entity entity);
	[[nodiscard]]
	Entity ReserveEntity();

	// Cancel a pending entity without allocating.
	void CancelEntityReservation(Entity entity) noexcept;

	void CommitEntity(Entity entity);
	void CommitEntity(Entity entity, SceneHandle handle);

private:
	std::uint32_t worldID_ = 0;

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
	// Vacant slots available for reuse; exhausted generations are excluded.
	std::vector<std::uint32_t> freeSceneIndices_;


	ECSCommandBuffer commandBuffer_;

private:
	// Friends
	friend class ECSCommandBuffer;
	friend void ECSAddWorldEntityCommandPayload::Execute(void* commandData, EntityRegistry& registry, World& world);
	friend void ECSAddWorldEntityCommandPayload::Destroy(void* commandData, World& world) noexcept;
	friend void ECSAddEntityCommandPayload::Execute(void* commandData, EntityRegistry& registry, World& world);
	friend void ECSAddEntityCommandPayload::Destroy(void* commandData, World& world) noexcept;
	friend void ECSRemoveEntityCommandPayload::Execute(void* commandData, EntityRegistry& registry, World& world);
	friend void ECSRemoveEntityCommandPayload::Destroy(void* commandData, World& world) noexcept;
};
