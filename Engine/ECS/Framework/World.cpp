#include "World.h"

Entity World::CreateEntity()
{
	return entityRegistry_.Create();
}

bool World::DestroyEntity(Entity entity)
{
	return entityRegistry_.Destroy(entity);
}

bool World::IsAlive(Entity entity) const noexcept
{
	return entityRegistry_.IsValid(entity);
}
