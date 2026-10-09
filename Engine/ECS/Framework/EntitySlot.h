#pragma once

#include <cstdint>

struct Archetype;

class Chunk;

enum class EntityState
{
	Invalid,
	Free,
	Reserved,
	Canceled,
	Alive,
};

struct EntitySlot
{
	// The generation advances when an entity is destroyed, before reuse.
	std::uint32_t generation = 0;
	// The archetype owning this entity's row.
	Archetype* archetype = nullptr;
	// The chunk containing the row.
	Chunk* chunk = nullptr;
	// The shared index into the chunk's Entity and component columns.
	std::uint32_t row = 0;
	// The state of the entity.
	EntityState state = EntityState::Free;
};