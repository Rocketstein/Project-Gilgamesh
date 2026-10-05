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
#include <memory>
#include <new>
#include <optional>
#include <stdexcept>
#include <typeinfo>
#include <unordered_map>
#include <utility>

#include "ComponentType.h"
#include "Engine/ECS/Entity/Entity.h"

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
    Chunk(const ChunkLayout& layout, const ComponentTypes& componentTypes);
    ~Chunk() noexcept;

    Chunk(const Chunk&) = delete;
    Chunk& operator=(const Chunk&) = delete;
    Chunk(Chunk&&) = delete;
    Chunk& operator=(Chunk&&) = delete;

    // Remove a live row; nullopt means it was the last row. Invalid rows throw.
    [[nodiscard]] std::optional<Entity> Erase(std::uint32_t row);

    // Report whether this chunk has room for another complete row.
    [[nodiscard]] bool IsFull() const noexcept { return count_ == layout_.capacity; }

    // Construct one component in the next row; callers manage pending cleanup.
    template <typename T, typename... Args>
    T& InsertComponent(Args&&... args)
    {
        if (storage_ == nullptr || count_ >= layout_.capacity)
        {
            throw std::length_error("Chunk is full and cannot accommodate more rows");
        }

        // Find the cell for T at the requested row.
        const auto it = layout_.componentColumns.find(ComponentTypeId{ typeid(T) });
        if (it == layout_.componentColumns.end())
        {
            throw std::out_of_range("Chunk does not contain the requested component type");
        }
        const auto& column = it->second;
        std::byte* address = storage_ + column.offset + count_ * column.stride;

        return *std::construct_at(reinterpret_cast<T*>(address), std::forward<Args>(args)...);
    }

    // Return an existing component in a live row of this chunk.
    template <typename T>
    T& Get(std::uint32_t row)
    {
        // Only committed rows contain constructed component objects.
        if (storage_ == nullptr || row >= count_)
        {
            throw std::out_of_range("Chunk row does not contain a live component");
        }

        // Locate T's column, then advance to the requested row within it.
        const auto columnIt = layout_.componentColumns.find(ComponentTypeId{ typeid(T) });
        if (columnIt == layout_.componentColumns.end())
        {
            throw std::out_of_range("Chunk does not contain the requested component type");
        }

        const ChunkLayout::Column& column = columnIt->second;
        std::byte* address = storage_ + column.offset + row * column.stride;
        return *std::launder(reinterpret_cast<T*>(address));
    }

private:
    // Layout metadata is owned by the archetype and outlives this chunk.
    const ChunkLayout& layout_;
    // Runtime lifetime operations are owned by the archetype.
    const ComponentTypes& componentTypes_;
    // Aligned storage holds the live component rows.
    std::byte* storage_ = nullptr;
    // Rows in [0, count_) contain live component values.
    std::uint32_t count_ = 0;
};
