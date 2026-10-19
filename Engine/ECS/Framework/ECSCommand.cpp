#include "ECSCommand.h"

#include "World.h"

// Apply scene changes through the same FIFO as entity structural commands.
void ECSSceneCommandPayload::Execute(void* commandData, EntityRegistry&, World& world)
{
	const auto* payload = static_cast<ECSSceneCommandPayload*>(commandData);
	world.ApplySceneCommand(payload->type, payload->scene);
}

// Scene handles own no resources, but their arena payload still has a lifetime.
void ECSSceneCommandPayload::Destroy(void* commandData, World&) noexcept
{
	std::destroy_at(static_cast<ECSSceneCommandPayload*>(commandData));
}

// ===============================================================================================
// Add Entity Command Payload
// ===============================================================================================
void ECSAddWorldEntityCommandPayload::Execute(void* commandData, EntityRegistry& registry, World& world)
{
	// Access the world-only payload through its concrete type.
	auto* payload = static_cast<ECSAddWorldEntityCommandPayload*>(commandData);
	world.CommitEntity(payload->entity);
}

void ECSAddWorldEntityCommandPayload::Destroy(void* commandData, World& world) noexcept
{
	// Cancel the reservation before destroying the concrete payload.
	auto* payload = static_cast<ECSAddWorldEntityCommandPayload*>(commandData);
	world.CancelEntityReservation(payload->entity);
	std::destroy_at(payload);
}

void ECSAddEntityCommandPayload::Execute(void* commandData, EntityRegistry& registry, World& world)
{
	auto* payload = static_cast<ECSAddEntityCommandPayload*>(commandData);
	world.CommitEntity(payload->entity, payload->handle);
}

void ECSAddEntityCommandPayload::Destroy(void* commandData, World& world) noexcept
{
	auto* payload = static_cast<ECSAddEntityCommandPayload*>(commandData);
	world.CancelEntityReservation(payload->entity);
	std::destroy_at(payload);
}

// ===============================================================================================
// Remove Entity Command Payload
// ===============================================================================================
void ECSRemoveEntityCommandPayload::Execute(void* commandData, EntityRegistry& registry, World& world)
{
	auto* payload = static_cast<ECSRemoveEntityCommandPayload*>(commandData);
	world.DestroyEntityNow(payload->entity);
}

void ECSRemoveEntityCommandPayload::Destroy(void* commandData, World& world) noexcept
{
	auto* payload = static_cast<ECSRemoveEntityCommandPayload*>(commandData);
	std::destroy_at(payload);
}
