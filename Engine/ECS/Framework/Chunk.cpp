#include "Chunk.h"

#include <bit>
#include <memory>
#include <new>
#include <optional>
#include <stdexcept>
#include <type_traits>

static_assert(std::is_nothrow_copy_constructible_v<Entity> &&
	std::is_nothrow_copy_assignable_v<Entity>,
	"Chunk swap-and-pop requires nothrow Entity copies");

namespace
{
	// Locate one value without treating individually constructed values as an array.
	std::byte* ComponentAddress(
		std::byte* storage,
		const ChunkLayout::Column& column,
		std::uint32_t row) noexcept
	{
		return storage + column.offset + static_cast<std::size_t>(row) * column.stride;
	}

	// Resolve an Entity value in an already committed row.
	Entity* EntityAddress(
		std::byte* storage,
		const ChunkLayout& layout,
		std::uint32_t row) noexcept
	{
		std::byte* address = storage + layout.entityOffset +
			static_cast<std::size_t>(row) * sizeof(Entity);
		return std::launder(reinterpret_cast<Entity*>(address));
	}
}

Chunk::Chunk(const ChunkLayout& layout, const ComponentTypes& componentTypes)
	: layout_(layout), componentTypes_(componentTypes)
{
	// Reject a layout that cannot address complete, aligned rows in this allocation.
	if (layout_.capacity == 0 || !std::has_single_bit(layout_.allocationAlignment) ||
		layout_.allocationAlignment < alignof(Entity) ||
		layout_.entityOffset > CHUNK_SIZE ||
		layout_.entityOffset % alignof(Entity) != 0 ||
		layout_.capacity > (CHUNK_SIZE - layout_.entityOffset) / sizeof(Entity) ||
		componentTypes_.size() != layout_.componentColumns.size())
	{
		throw std::invalid_argument("Chunk layout does not describe valid rows");
	}
	for (const auto& [typeId, info] : componentTypes_)
	{
		const auto columnIt = layout_.componentColumns.find(typeId);
		if (columnIt == layout_.componentColumns.end() ||
			info.size == 0 || !std::has_single_bit(info.alignment) ||
			layout_.allocationAlignment < info.alignment ||
			info.moveConstruct == nullptr || info.destroy == nullptr)
		{
			throw std::invalid_argument("Chunk is missing component layout or lifetime operations");
		}

		const ChunkLayout::Column& column = columnIt->second;
		if (column.stride < info.size || column.stride == 0 ||
			column.offset > CHUNK_SIZE ||
			column.offset % info.alignment != 0 ||
			column.stride % info.alignment != 0 ||
			layout_.capacity > (CHUNK_SIZE - column.offset) / column.stride)
		{
			throw std::invalid_argument("Chunk component column exceeds its aligned storage");
		}
	}

	// Every column uses this one allocation, including over-aligned components.
	storage_ = static_cast<std::byte*>(
		::operator new(CHUNK_SIZE, std::align_val_t{ layout_.allocationAlignment }));
}

Chunk::~Chunk() noexcept
{
	// Only committed rows contain complete objects that need destruction.
	for (std::uint32_t remaining = count_; remaining > 0; --remaining)
	{
		const std::uint32_t row = remaining - 1;
		for (const auto& [typeId, info] : componentTypes_)
		{
			const ChunkLayout::Column& column = layout_.componentColumns.at(typeId);
			info.destroy(ComponentAddress(storage_, column, row));
		}
		std::destroy_at(EntityAddress(storage_, layout_, row));
	}

	::operator delete(storage_, std::align_val_t{ layout_.allocationAlignment });
}

std::optional<Entity> Chunk::Erase(std::uint32_t row)
{
	if (row >= count_)
	{
		throw std::out_of_range("Chunk row does not contain a live entity");
	}

	const std::uint32_t lastRow = count_ - 1;
	Entity* lastEntity = EntityAddress(storage_, layout_, lastRow);
	std::optional<Entity> displaced;
	if (row != lastRow)
	{
		displaced.emplace(*lastEntity);
	}

	// Destroy the removed row; move the last value into its cell when needed.
	for (const auto& [typeId, info] : componentTypes_)
	{
		const ChunkLayout::Column& column = layout_.componentColumns.at(typeId);
		void* destination = ComponentAddress(storage_, column, row);
		info.destroy(destination);
		if (row != lastRow)
		{
			void* source = ComponentAddress(storage_, column, lastRow);
			info.moveConstruct(destination, source);
			info.destroy(source);
		}
	}

	// The Entity column follows the same swap-and-pop row mapping.
	if (row != lastRow)
	{
		Entity* destination = EntityAddress(storage_, layout_, row);
		*destination = *lastEntity;
	}
	std::destroy_at(lastEntity);
	--count_;
	return displaced;
}
