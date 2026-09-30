#pragma once
#include <cstdint>
#include <vector>

#include "ComponentType.h"
#include "Engine/ECS/Entity/Entity.h"

constexpr size_t CHUNK_SIZE = 16384;

class Chunk final
{
public:
    explicit Chunk(const ComponentTypes& layout);
    ~Chunk();

    Chunk(const Chunk&) = delete;
    Chunk& operator=(const Chunk&) = delete;
    Chunk(Chunk&&) = delete;
    Chunk& operator=(Chunk&&) = delete;

private:
    const ComponentTypes* layout_;
    std::byte* storage_ = nullptr;
    std::uint32_t count_ = 0;
};