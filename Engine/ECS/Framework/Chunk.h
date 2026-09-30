#pragma once
#include <cstddef>
#include <cstdint>

#include "ComponentType.h"

constexpr std::size_t CHUNK_SIZE = 16384;

// Describes one fixed-size block of component rows using an archetype's layout.
class Chunk final
{
public:
    explicit Chunk(const ComponentTypes& layout);
    // Storage allocation and cleanup will be added with the chunk implementation.
    ~Chunk() = default;

    Chunk(const Chunk&) = delete;
    Chunk& operator=(const Chunk&) = delete;
    Chunk(Chunk&&) = delete;
    Chunk& operator=(Chunk&&) = delete;

private:
    // Layout metadata is owned by the archetype and outlives this chunk.
    const ComponentTypes* layout_;
    // Aligned storage will hold the live component rows.
    std::byte* storage_ = nullptr;
    // Rows in [0, count_) contain live component values.
    std::uint32_t count_ = 0;
};
