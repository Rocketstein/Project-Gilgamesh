#include "World.h"

Entity World::CreateEntity()
{
	Entity entity;

	return entity;
}

bool World::DestroyEntity(Entity entity)
{
	return true;
}

bool World::IsAlive(Entity entity) const noexcept
{
	return entityRegistery_.IsValid(entity);
}