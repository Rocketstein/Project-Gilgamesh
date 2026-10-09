#include "ECSCommand.h"

#include "World.h"

// ===============================================================================================
// Add Entity Command Payload
// ===============================================================================================
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
	auto* payload = static_cast<ECSAddEntityCommandPayload*>(commandData);
	world.DestroyEntity(payload->entity);
}

void ECSRemoveEntityCommandPayload::Destroy(void* commandData, World& world)
{
	auto* payload = static_cast<ECSAddEntityCommandPayload*>(commandData);
	std::destroy_at(payload);
}