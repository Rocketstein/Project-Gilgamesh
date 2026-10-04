/*
 * A chunk is designed to store one archetype's entities as columns (SoA):
 *
 *   Entity:    [e0][e1]...
 *   Position:  [p0][p1]...
 *   Flag:      [f0][f1]...
 *   padding, if needed for alignment
 *   Transform: [t0][t1]...
 *
 * The same index across columns is one logical row: e1, p1, f1, and t1
 * belong to the same entity. Those values need not be adjacent in memory.
 * Each chunk owns its storage and live row count. Chunks of one archetype
 * share a ChunkLayout containing capacity, column offsets and strides, and
 * the alignment required for the storage allocation.
 */
#pragma once
#include <cstddef>
#include <cstdint>
#include <unordered_map>

#include "ComponentType.h"

constexpr std::size_t CHUNK_SIZE = 16384;

// Describes the column layout shared by every chunk of one archetype.
struct ChunkLayout
{
    // Locates consecutive values of one component type in the chunk allocation.
    struct Column
    {
        // Byte offset of the first value from the allocation base.
        std::size_t offset;
        // Byte distance between values at adjacent row indices.
        std::size_t stride;
    };

    // Maximum number of entity rows that fit in one chunk.
    std::uint32_t capacity;
    // Alignment required for the chunk allocation's base address.
    std::size_t allocationAlignment;
    // Byte offset of the first Entity value from the allocation base.
    std::size_t entityOffset;
    // Component column locations indexed by their type IDs.
    std::unordered_map<ComponentTypeId, Column> componentColumns;
};

// Describes one fixed-size block of component rows using an archetype's layout.
class Chunk final
{
public:
    explicit Chunk(const ChunkLayout& layout);
    // Storage allocation and cleanup will be added with the chunk implementation.
    ~Chunk() = default;

    Chunk(const Chunk&) = delete;
    Chunk& operator=(const Chunk&) = delete;
    Chunk(Chunk&&) = delete;
    Chunk& operator=(Chunk&&) = delete;

private:
    // Layout metadata is owned by the archetype and outlives this chunk.
    const ChunkLayout& layout_;
    // Aligned storage will hold the live component rows.
    std::byte* storage_ = nullptr;
    // Rows in [0, count_) contain live component values.
    std::uint32_t count_ = 0;
};
