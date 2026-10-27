#include "World.h"

#include "IDGenerator.h"

#include <stdexcept>

#include "Engine/Core/Logging/Logger.h"

// Initialize generic scheduling roots without registering concrete engine systems.
World::World() : worldID_(IDGenerator<WorldTag>::Next()), entityRegistry_(worldID_), commandBuffer_(*this)
{
	for (SystemGroup* group : { &simulationSystems_, &transformSystems_, &extractionSystems_ })
	{
		group->Initialize();
		group->SetEnabled(true);
	}
}

// Tear systems down while the registry and queued component payloads still exist.
World::~World() noexcept
{
	phase_ = ExecutionPhase::Shutdown;
	SetSchedulingMutationAllowed(false);
	for (SystemGroup* group : { &extractionSystems_, &transformSystems_, &simulationSystems_ })
	{
		try
		{
			group->ShutdownOwned(*this);
		}
		catch (...)
		{
			GILGAMESH_LOG(Runtime, Error, "A system failed during world shutdown");
		}
	}

	// Honor deferred cleanup recorded by OnDestroy before registry destruction.
	try
	{
		ApplyPendingCommands();
	}
	catch (...)
	{
		GILGAMESH_LOG(Runtime, Error, "World cleanup command playback failed");
	}
	// Later member destructors must not record into an already destroyed command buffer.
	phase_ = ExecutionPhase::ShutdownComplete;
}

// Keep every root immutable while any world phase is executing.
void World::SetSchedulingMutationAllowed(bool allowed) noexcept
{
	simulationSystems_.SetMutationAllowed(allowed);
	transformSystems_.SetMutationAllowed(allowed);
	extractionSystems_.SetMutationAllowed(allowed);
}

// Immediate scene operations and nested world updates would invalidate traversal.
void World::RequireIdle() const
{
	if (phase_ == ExecutionPhase::Idle) return;
	GILGAMESH_LOG(Runtime, Error, "This world operation requires an idle execution phase");
	throw std::logic_error("World operation requires an idle execution phase");
}

// Presentation reads stable storage, and playback must not append to its own FIFO.
void World::RequireStructuralRequestsAllowed() const
{
	if (phase_ == ExecutionPhase::Idle || phase_ == ExecutionPhase::Simulation ||
		phase_ == ExecutionPhase::Shutdown)
	{
		return;
	}
	GILGAMESH_LOG(Runtime, Error, "Structural requests are not allowed during this world phase");
	throw std::logic_error("Structural requests are not allowed during this world phase");
}

// Mark playback explicitly so component callbacks cannot recursively mutate the queue.
void World::ApplyPendingCommands()
{
	const ExecutionPhase previousPhase = phase_;
	phase_ = ExecutionPhase::Playback;
	try
	{
		commandBuffer_.Flush();
	}
	catch (...)
	{
		phase_ = previousPhase;
		throw;
	}
	phase_ = previousPhase;
}

// Simulation changes become visible before later transform and extraction phases.
void World::Update(float deltaTime)
{
	RequireIdle();
	SetSchedulingMutationAllowed(false);
	try
	{
		ApplyPendingCommands();
		phase_ = ExecutionPhase::Simulation;
		if (!simulationPaused_) simulationSystems_.Update(*this, deltaTime);
		ApplyPendingCommands();
	}
	catch (...)
	{
		phase_ = ExecutionPhase::Idle;
		SetSchedulingMutationAllowed(true);
		throw;
	}
	phase_ = ExecutionPhase::Idle;
	SetSchedulingMutationAllowed(true);
}

// Run once after editor changes, even when simulation is paused.
void World::PreparePresentation(float realDeltaTime)
{
	RequireIdle();
	SetSchedulingMutationAllowed(false);
	try
	{
		ApplyPendingCommands();
		phase_ = ExecutionPhase::Transform;
		transformSystems_.Update(*this, realDeltaTime);
		phase_ = ExecutionPhase::Extraction;
		extractionSystems_.Update(*this, realDeltaTime);
	}
	catch (...)
	{
		phase_ = ExecutionPhase::Idle;
		SetSchedulingMutationAllowed(true);
		throw;
	}
	phase_ = ExecutionPhase::Idle;
	SetSchedulingMutationAllowed(true);
}

// Synchronous creation is reserved for tests and controlled initialization.
SceneHandle World::CreateScene(std::string name)
{
	RequireIdle();
	const SceneHandle handle = ReserveScene();
	try
	{
		CommitScene(handle, std::move(name));
	}
	catch (...)
	{
		CancelSceneReservation(handle);
		throw;
	}
	return handle;
}

// Allocate an unpublished identity with capacity for allocation-free cancellation.
SceneHandle World::ReserveScene()
{
	const bool reuseSlot = !freeSceneIndices_.empty();
	if (!reuseSlot)
	{
		if (scenes_.size() >= std::numeric_limits<std::uint32_t>::max())
			throw std::length_error("World scene handle capacity exceeded");

		// Every issued slot can return here from noexcept payload cleanup.
		freeSceneIndices_.reserve(scenes_.size() + 1);
		const auto id = static_cast<std::uint32_t>(scenes_.size());
		scenes_.push_back(SceneItem{ nullptr, 0, true });
		return SceneHandle{ id, 0, worldID_ };
	}

	const std::uint32_t id = freeSceneIndices_.back();
	freeSceneIndices_.pop_back();
	SceneItem& item = scenes_[id];
	item.reserved = true;
	return SceneHandle{ id, item.generation, worldID_ };
}

// Publish only after constructing the scene and its owned name successfully.
void World::CommitScene(SceneHandle handle, std::string name)
{
	if (!IsSceneHandleAllocated(handle) || !scenes_[handle.id].reserved)
	{
		GILGAMESH_LOG(Runtime, Warning, "Discarded creation for a cancelled scene reservation");
		return;
	}
	std::string sceneName = name.empty() ? "Scene_" + std::to_string(handle.id) : std::move(name);
	auto scene = std::make_unique<Scene>(std::move(sceneName));
	SceneItem& item = scenes_[handle.id];
	item.scene = std::move(scene);
	item.reserved = false;
}

// Advance generation so an aborted creation cannot address a later slot occupant.
void World::CancelSceneReservation(SceneHandle handle) noexcept
{
	if (!IsSceneHandleAllocated(handle) || !scenes_[handle.id].reserved) return;
	SceneItem& item = scenes_[handle.id];
	item.reserved = false;
	if (item.generation < std::numeric_limits<std::uint32_t>::max())
	{
		++item.generation;
		if (item.generation < std::numeric_limits<std::uint32_t>::max())
			freeSceneIndices_.push_back(handle.id);
	}
}

// Return an identity now while deferring the actual object until command playback.
SceneHandle World::RequestCreateScene(std::string name)
{
	RequireStructuralRequestsAllowed();
	return commandBuffer_.CreateScene(std::move(name));
}

// Synchronous destruction is reserved for tests and controlled initialization.
bool World::DestroyScene(SceneHandle handle)
{
	RequireIdle();
	// Component destructors are callbacks too; keep their reentry out of live storage.
	SetSchedulingMutationAllowed(false);
	phase_ = ExecutionPhase::Playback;
	try
	{
		const bool destroyed = DestroySceneNow(handle);
		phase_ = ExecutionPhase::Idle;
		SetSchedulingMutationAllowed(true);
		return destroyed;
	}
	catch (...)
	{
		phase_ = ExecutionPhase::Idle;
		SetSchedulingMutationAllowed(true);
		throw;
	}
}

// Systems request destruction through the command buffer instead of this helper.
bool World::DestroySceneNow(SceneHandle handle)
{
	// An unpublished reservation can be cancelled without constructing a scene.
	if (IsSceneHandleAllocated(handle) && scenes_[handle.id].reserved)
	{
		CancelSceneReservation(handle);
		return true;
	}
	if (!IsSceneHandleValid(handle)) return false;
	Scene* scene = scenes_[handle.id].scene.get();

	// Allocate recycling space before destroying any scene content.
	const bool canReuse = scenes_[handle.id].generation <
		std::numeric_limits<std::uint32_t>::max() - 1;
	if (canReuse)
	{
		freeSceneIndices_.reserve(freeSceneIndices_.size() + 1);
	}

	// Clear selection before retiring the slot; never choose an implicit fallback.
	if (activeScene_ == handle) activeScene_ = {};

	// DestroyEntity erases membership, so drain rather than iterate the vector.
	while (!scene->entities_.empty())
	{
		DestroyEntityNow(scene->entities_.back());
	}

	// Reacquire the slot after component destruction and retire exhausted slots.
	SceneItem& item = scenes_[handle.id];
	item.scene.reset();
	++item.generation;
	if (canReuse)
	{
		freeSceneIndices_.push_back(handle.id);
	}
	return true;
}

// Record only a currently valid selection, then validate it again at playback.
bool World::RequestSetActiveScene(SceneHandle handle)
{
	RequireStructuralRequestsAllowed();
	if (!IsSceneHandleAllocated(handle))
	{
		GILGAMESH_LOG(Runtime, Warning, "Cannot select an invalid or foreign scene");
		return false;
	}
	commandBuffer_.RecordSceneCommand(ECSSceneCommandType::SetActive, handle);
	return true;
}

// An explicit clear is distinct from an invalid selection request.
void World::RequestClearActiveScene()
{
	RequireStructuralRequestsAllowed();
	commandBuffer_.RecordSceneCommand(ECSSceneCommandType::ClearActive, {});
}

// The return value acknowledges the request, not immediate destruction.
bool World::RequestDestroyScene(SceneHandle handle)
{
	RequireStructuralRequestsAllowed();
	if (!IsSceneHandleAllocated(handle))
	{
		GILGAMESH_LOG(Runtime, Warning, "Cannot destroy an invalid or foreign scene");
		return false;
	}
	commandBuffer_.RecordSceneCommand(ECSSceneCommandType::Destroy, handle);
	return true;
}

// Scene operations retain their relative order with all entity edits.
void World::ApplySceneCommand(ECSSceneCommandType type, SceneHandle handle, std::string name)
{
	if (type == ECSSceneCommandType::Create)
	{
		CommitScene(handle, std::move(name));
		return;
	}
	if (type == ECSSceneCommandType::ClearActive)
	{
		activeScene_ = {};
		return;
	}
	if (!IsSceneHandleValid(handle))
	{
		GILGAMESH_LOG(Runtime, Warning, "Discarded a scene request whose handle became stale");
		return;
	}
	if (type == ECSSceneCommandType::SetActive)
		activeScene_ = handle;
	else if (type == ECSSceneCommandType::Destroy)
		DestroySceneNow(handle);
}

// Scene pointers are borrowed only until the next structural playback boundary.
const Scene* World::TryGetActiveScene() const noexcept
{
	return TryGetScene(activeScene_);
}

const Scene* World::TryGetScene(SceneHandle handle) const noexcept
{
	if (!IsSceneHandleValid(handle)) return nullptr;
	return scenes_[handle.id].scene.get();
}

bool World::IsSceneHandleValid(SceneHandle handle) const noexcept
{
	return IsSceneHandleAllocated(handle) && scenes_[handle.id].scene != nullptr;
}

// Request validation includes reserved slots without exposing them as live scenes.
bool World::IsSceneHandleAllocated(SceneHandle handle) const noexcept
{
	// Reject foreign, null, or out-of-range handles before accessing the slot.
	if (handle.owningWorldID_ != worldID_ || handle.IsNull())
	{
		return false;
	}

	if (handle.id >= scenes_.size())
	{
		return false;
	}

	const SceneItem& item = scenes_[handle.id];
	return item.generation == handle.generation && (item.reserved || item.scene != nullptr);
}

Entity World::CreateEntity()
{
	RequireStructuralRequestsAllowed();
	return commandBuffer_.CreateEntity();
}

Entity World::CreateEntity(SceneHandle handle)
{
	RequireStructuralRequestsAllowed();
	return commandBuffer_.CreateEntity(handle);
}

void World::RemoveEntity(Entity entity)
{
	RequireStructuralRequestsAllowed();
	commandBuffer_.RemoveEntity(entity);
}

Entity World::CreateEntityNow()
{
	return entityRegistry_.Create();
}

Entity World::CreateEntityNow(SceneHandle handle)
{
	if (!IsSceneHandleValid(handle)) return {};

	Scene* scene = scenes_[handle.id].scene.get();
	const Entity entity = entityRegistry_.Create();
	// Publish both membership directions, retiring the entity if tracking fails.
	try
	{
		entityScenes_.emplace(entity, handle);
		scene->entities_.push_back(entity);
	}
	catch (...)
	{
		entityScenes_.erase(entity);
		entityRegistry_.Destroy(entity);
		throw;
	}
	return entity;
}

Entity World::ReserveEntity()
{
	return entityRegistry_.Reserve();
}

void World::CancelEntityReservation(Entity entity) noexcept
{
	if (entity.IsNull()) return;
	entityRegistry_.CancelEntityReservation(entity);
}

void World::CommitEntity(Entity entity)
{
	if (entityRegistry_.GetEntityState(entity) != EntityState::Reserved) return;
	entityRegistry_.Commit(entity);
}

// Activate a reserved entity after registering its scene membership.
void World::CommitEntity(Entity entity, SceneHandle handle)
{
	// Only a pending reservation can acquire scene membership.
	if (entityRegistry_.GetEntityState(entity) != EntityState::Reserved) return;
	if (!IsSceneHandleValid(handle))
	{
		CancelEntityReservation(entity);
		return;
	}

	// Publish both membership directions before making the entity alive.
	Scene* scene = scenes_[handle.id].scene.get();
	try
	{
		entityScenes_.emplace(entity, handle);
		scene->entities_.push_back(entity);
	}
	catch (...)
	{
		// Roll back tracking and cancel the reservation if allocation fails.
		entityScenes_.erase(entity);
		CancelEntityReservation(entity);
		throw;
	}

	entityRegistry_.Commit(entity);
}

void World::DestroyEntityNow(Entity entity)
{
	// Copy the owner before component destruction can create other entities.
	const auto membership = entityScenes_.find(entity);
	const SceneHandle owner = membership != entityScenes_.end()
		? membership->second : SceneHandle{};
	if (!entityRegistry_.Destroy(entity)) return;

	// World-owned entities have no scene, while scene members leave both indices.
	if (IsSceneHandleValid(owner))
	{
		std::erase(scenes_[owner.id].scene->entities_, entity);
	}
	entityScenes_.erase(entity);
}

bool World::IsAlive(Entity entity) const noexcept
{
	return entityRegistry_.GetEntityState(entity) == EntityState::Alive;
}
