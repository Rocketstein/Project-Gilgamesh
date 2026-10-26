#pragma once

#include <functional>
#include <string>

#include "ECSCommandBuffer.h"
#include "EntityRegistry.h"
#include "Scene.h"
#include "Engine/ECS/System/Core/SystemGroup.h"

// Owns ECS storage and coordinates entity lifetimes with scene membership.
class World final
{
public:
	World();
	~World() noexcept;

	World(const World&) = delete;
	World& operator=(const World&) = delete;

	// Apply queued changes around simulation; call only outside active ECS traversals.
	void Update(float deltaTime);
	// Finalize transforms and extract presentation data once per rendered frame.
	// Call only outside active ECS traversals, after the frame's editor changes.
	void PreparePresentation(float realDeltaTime);

	void SetSimulationPaused(bool paused) noexcept { simulationPaused_ = paused; }
	[[nodiscard]] bool IsSimulationPaused() const noexcept { return simulationPaused_; }

	// Groups own registered systems; callers retain only borrowed references.
	[[nodiscard]] SystemGroup& GetSimulationSystems() noexcept { return simulationSystems_; }
	[[nodiscard]] SystemGroup& GetTransformSystems() noexcept { return transformSystems_; }
	[[nodiscard]] SystemGroup& GetExtractionSystems() noexcept { return extractionSystems_; }

	// Immediate execution is for initialization, tests, and other controlled setup only.
	// Never call these during entity/scene traversal; normal changes use request APIs.
	[[nodiscard]] SceneHandle CreateScene(std::string name = "");
	// Immediate destruction is likewise limited to initialization, tests, and controlled setup.
	// It can cancel a pending reservation, but must never run during a traversal.
	bool DestroyScene(SceneHandle handle);

	// Creation reserves an identity immediately; the scene is published only at playback.
	[[nodiscard]] SceneHandle RequestCreateScene(std::string name = "");
	// Selection and destruction accept live or reserved identities in the same FIFO.
	bool RequestSetActiveScene(SceneHandle handle);
	void RequestClearActiveScene();
	bool RequestDestroyScene(SceneHandle handle);
	[[nodiscard]] SceneHandle GetActiveSceneHandle() const noexcept { return activeScene_; }
	[[nodiscard]] const Scene* TryGetActiveScene() const noexcept;
	[[nodiscard]] const Scene* TryGetScene(SceneHandle handle) const noexcept;
	[[nodiscard]] bool IsSceneHandleValid(SceneHandle handle) const noexcept;

	[[nodiscard]]
	bool IsAlive(Entity entity) const noexcept;

	// Visit all live committed world entities, including those without a scene; exclude reservations.
	// The visitor must not structurally mutate this World. Revalidate retained handles later.
	void ForEachEntity(const std::function<void(Entity)>& visitor) const;

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
		RequireStructuralRequestsAllowed();
		commandBuffer_.RemoveComponent<Component>(entity);
	}

	template <typename Component, typename... Args>
	void Emplace(Entity entity, Args&&... args)
	{
		RequireStructuralRequestsAllowed();
		commandBuffer_.EmplaceComponent<Component>(
			entity, std::forward<Args>(args)...);
	}

private:
	// Distinguishes write phases from transform and extraction read boundaries.
	enum class ExecutionPhase
	{
		Idle,
		Playback,
		Simulation,
		Transform,
		Extraction,
		Shutdown,
		ShutdownComplete
	};

	// Guards callbacks against reentry and unsafe structural or scene mutations.
	void RequireIdle() const;
	void RequireStructuralRequestsAllowed() const;
	void SetSchedulingMutationAllowed(bool allowed) noexcept;
	void ApplyPendingCommands();
	void ApplySceneCommand(ECSSceneCommandType type, SceneHandle handle, std::string name);
	[[nodiscard]] SceneHandle ReserveScene();
	void CommitScene(SceneHandle handle, std::string name);
	void CancelSceneReservation(SceneHandle handle) noexcept;
	[[nodiscard]] bool IsSceneHandleAllocated(SceneHandle handle) const noexcept;
	bool DestroySceneNow(SceneHandle handle);

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
	ExecutionPhase phase_ = ExecutionPhase::Idle; // Outlives storage to guard destructor callbacks.
	bool simulationPaused_ = false; // Skips simulation without stopping presentation.

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
		// Reserved slots have an identity but are not visible through TryGetScene yet.
		bool reserved = false;
	};
	// Slots keep their indices across reuse and are addressed by SceneHandle::id.
	std::vector<SceneItem> scenes_;
	// Vacant slots available for reuse; exhausted generations are excluded.
	std::vector<std::uint32_t> freeSceneIndices_;

	// Default render source; selection does not silently filter global ECS queries.
	SceneHandle activeScene_;

	ECSCommandBuffer commandBuffer_;
	SystemGroup simulationSystems_; // Gameplay systems in registration order.
	SystemGroup transformSystems_; // Final world-transform evaluation.
	SystemGroup extractionSystems_; // Read-only render and presentation extraction.

private:
	// Friends
	friend class ECSCommandBuffer;
	friend struct ECSSceneCommandPayload;
	friend struct ECSAddWorldEntityCommandPayload;
	friend struct ECSAddEntityCommandPayload;
	friend struct ECSRemoveEntityCommandPayload;
};
