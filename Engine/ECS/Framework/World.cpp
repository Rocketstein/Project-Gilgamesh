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
	if (phase_ == ExecutionPhase::Idle && traversalDepth_ == 0) return;
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

// Preserve synchronous scene construction for setup outside running systems.
SceneHandle World::CreateScene(std::string name)
{
	RequireIdle();
	// Reserve the null index and narrow only after checking the slot capacity.
	const bool reuseSlot = !freeSceneIndices_.empty();
	if (!reuseSlot && scenes_.size() >= std::numeric_limits<std::uint32_t>::max())
	{
		throw std::length_error("World scene handle capacity exceeded");
	}
	const std::uint32_t id = reuseSlot
		? freeSceneIndices_.back()
		: static_cast<std::uint32_t>(scenes_.size());

	// Construct the scene before changing either the slots or the free list.
	std::string sceneName = name.empty() ? "Scene_" + std::to_string(id) : std::move(name);
	auto scene = std::make_unique<Scene>(std::move(sceneName));
	if (reuseSlot)
	{
		scenes_[id].scene = std::move(scene);
		freeSceneIndices_.pop_back();
	}
	else
	{
		scenes_.push_back(SceneItem{ std::move(scene), 0 });
	}

	return SceneHandle{ id, scenes_[id].generation, worldID_ };
}

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
	if (!IsSceneHandleValid(handle))
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
	if (!IsSceneHandleValid(handle))
	{
		GILGAMESH_LOG(Runtime, Warning, "Cannot destroy an invalid or foreign scene");
		return false;
	}
	commandBuffer_.RecordSceneCommand(ECSSceneCommandType::Destroy, handle);
	return true;
}

// Scene operations retain their relative order with all entity edits.
void World::ApplySceneCommand(ECSSceneCommandType type, SceneHandle handle)
{
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
	// Reject foreign, null, or out-of-range handles before accessing the slot.
	if (handle.owningWorldID_ != worldID_ || handle.IsNull())
	{
		return false;
	}

	if (handle.id >= scenes_.size())
	{
		return false;
	}

	// A recycled slot has no scene until it is reused.
	if (scenes_[handle.id].scene == nullptr)
	{
		return false;
	}

	// A matching index alone does not identify the same scene after slot reuse.
	if (scenes_[handle.id].generation != handle.generation)
	{
		return false;
	}

	return true;
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
