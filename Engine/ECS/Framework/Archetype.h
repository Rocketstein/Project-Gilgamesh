#pragma once

#include <memory>
#include <unordered_map>
#include <vector>

#include "Chunk.h"
#include "Engine/ECS/Entity/Entity.h"

struct Archetype
{
	std::vector<std::unique_ptr<Chunk>> chunks;
};