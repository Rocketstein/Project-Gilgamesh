#include "ECSCommand.h"

#include "World.h"

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