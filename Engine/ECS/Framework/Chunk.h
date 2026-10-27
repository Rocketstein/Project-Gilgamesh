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
#include <vector>

#include "ArchetypeResource.h"
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
    // Owns the objects constructed for one row until the archetype commits it.
    class PendingRow final
    {
    public:
        ~PendingRow() noexcept;

        PendingRow(const PendingRow&) = delete;
        PendingRow& operator=(const PendingRow&) = delete;
        PendingRow(PendingRow&&) = delete;
        PendingRow& operator=(PendingRow&&) = delete;

        // Construct a new component directly in this uncommitted row.
        template <typename T, typename... Args>
        T& Construct(Args&&... args)
        {
            const ComponentTypeId typeId{ typeid(T) };
            EnsureCanConstruct(typeId);

            // Record the value only after its constructor has succeeded.
            T& value = chunk_->InsertComponent<T>(std::forward<Args>(args)...);
            try
            {
                constructedTypes_.push_back(typeId);
            }
            catch (...)
            {
                std::destroy_at(std::addressof(value));
                throw;
            }
            return value;
        }

    private:
        friend class Chunk;
        friend struct Archetype;
        friend class EntityRegistry;

        PendingRow(Chunk& chunk, Entity entity);

        // Reject missing or duplicate component columns before construction.
        void EnsureCanConstruct(ComponentTypeId typeId) const;
        // Only registry migration may move an existing component into this row.
        void MoveFrom(ComponentTypeId typeId, Chunk& source, std::uint32_t sourceRow);
        [[nodiscard]] bool IsComplete() const noexcept;
        std::uint32_t Commit() noexcept;

        // A null pointer means the row has been committed.
        Chunk* chunk_ = nullptr;
        // The row remains outside [0, count_) until Commit succeeds.
        std::uint32_t row_ = 0;
        // Lists the component objects to destroy if the insertion is abandoned.
        std::vector<ComponentTypeId> constructedTypes_;
    };

    Chunk(const ChunkLayout& layout, const ComponentTypes& componentTypes);
    ~Chunk() noexcept;

    Chunk(const Chunk&) = delete;
    Chunk& operator=(const Chunk&) = delete;
    Chunk(Chunk&&) = delete;
    Chunk& operator=(Chunk&&) = delete;

    // Report whether this chunk has room for another complete row.
    [[nodiscard]] bool IsFull() const noexcept { return count_ == layout_.capacity; }

    // Count only rows whose Entity and component columns are complete.
    [[nodiscard]] std::uint32_t Count() const noexcept { return count_; }

    // Copy the complete entity identity stored alongside one committed component row.
    [[nodiscard]] Entity GetEntity(std::uint32_t row) const;

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

    template <typename T>
    const T& Get(std::uint32_t row) const
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
    friend struct Archetype;

    // Remove a live row; the archetype must update its availability list.
    [[nodiscard]] std::optional<Entity> Erase(std::uint32_t row);

    // Open an uncommitted row; only the owning archetype may start it.
    [[nodiscard]] PendingRow BeginRow(Entity entity);

    // Resolve a constructed component in a committed source row.
    [[nodiscard]] void* GetComponentAddress(ComponentTypeId typeId, std::uint32_t row);

    // Construct one component in the next row for its pending transaction.
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
    // Layout metadata is owned by the archetype and outlives this chunk.
    const ChunkLayout& layout_;
    // Runtime lifetime operations are owned by the archetype.
    const ComponentTypes& componentTypes_;
    // Aligned storage holds the live component rows.
    std::byte* storage_ = nullptr;
    // Rows in [0, count_) contain live component values.
    std::uint32_t count_ = 0;
};
