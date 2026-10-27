#include "Chunk.h"

#include <algorithm>
#include <cassert>
#include <bit>
#include <memory>
#include <new>
#include <optional>
#include <stdexcept>
#include <type_traits>
#include <utility>

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
} // Anonymous namespace

Chunk::PendingRow::PendingRow(Chunk& chunk, Entity entity)
	: row_(chunk.count_)
{
	if (chunk.IsFull())
	{
		throw std::length_error("Chunk is full and cannot accommodate more rows");
	}

	// Prepare rollback bookkeeping before any object is constructed.
	constructedTypes_.reserve(chunk.componentTypes_.size());
	std::byte* address = chunk.storage_ + chunk.layout_.entityOffset +
		static_cast<std::size_t>(row_) * sizeof(Entity);
	std::construct_at(reinterpret_cast<Entity*>(address), entity);
	chunk_ = &chunk;
}

Chunk::PendingRow::~PendingRow() noexcept
{
	if (chunk_ == nullptr)
	{
		return;
	}

	// An abandoned row owns only the component objects listed here.
	for (auto it = constructedTypes_.rbegin(); it != constructedTypes_.rend(); ++it)
	{
		const ComponentTypeInfo& info = chunk_->componentTypes_.at(*it);
		const ChunkLayout::Column& column = chunk_->layout_.componentColumns.at(*it);
		info.destroy(ComponentAddress(chunk_->storage_, column, row_));
	}
	std::destroy_at(EntityAddress(chunk_->storage_, chunk_->layout_, row_));
}

void Chunk::PendingRow::EnsureCanConstruct(ComponentTypeId typeId) const
{
	if (chunk_ == nullptr)
	{
		throw std::logic_error("Chunk row is no longer pending");
	}
	if (!chunk_->layout_.componentColumns.contains(typeId))
	{
		throw std::out_of_range("Chunk does not contain the requested component type");
	}
	if (std::find(constructedTypes_.begin(), constructedTypes_.end(), typeId) !=
		constructedTypes_.end())
	{
		throw std::logic_error("Component was already constructed in this row");
	}
}

void Chunk::PendingRow::MoveFrom(
	ComponentTypeId typeId, Chunk& source, std::uint32_t sourceRow)
{
	EnsureCanConstruct(typeId);

	// Resolve both cells before moving, since the registered move cannot throw.
	void* sourceAddress = source.GetComponentAddress(typeId, sourceRow);
	const ComponentTypeInfo& info = chunk_->componentTypes_.at(typeId);
	const ChunkLayout::Column& column = chunk_->layout_.componentColumns.at(typeId);
	void* destination = ComponentAddress(chunk_->storage_, column, row_);
	constructedTypes_.push_back(typeId);
	info.moveConstruct(destination, sourceAddress);
}

bool Chunk::PendingRow::IsComplete() const noexcept
{
	return chunk_ != nullptr &&
		constructedTypes_.size() == chunk_->componentTypes_.size();
}

std::uint32_t Chunk::PendingRow::Commit() noexcept
{
	assert(IsComplete());
	Chunk* chunk = std::exchange(chunk_, nullptr);
	return chunk->count_++;
}

Chunk::PendingRow Chunk::BeginRow(Entity entity)
{
	return PendingRow(*this, entity);
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

	// Allocate version entries before raw storage so a metadata allocation cannot leak it.
	changeVersions_.reserve(componentTypes_.size());
	for (const auto& entry : componentTypes_)
	{
		changeVersions_.emplace(entry.first, 0);
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

// Read identity storage without exposing a mutable reference to the chunk's Entity column.
Entity Chunk::GetEntity(std::uint32_t row) const
{
	if (row >= count_)
	{
		throw std::out_of_range("Chunk row does not contain a live entity");
	}
	return *EntityAddress(storage_, layout_, row);
}

// Read a column's last possible write without changing observer state.
std::uint64_t Chunk::GetChangeVersion(ComponentTypeId typeId) const
{
	return changeVersions_.at(typeId);
}

// Only existing columns can be stamped; all metadata is allocated at construction.
void Chunk::MarkChanged(ComponentTypeId typeId, std::uint64_t version)
{
	changeVersions_.at(typeId) = version;
}

// Row insertion, migration, and removal can change every column's visible rows.
void Chunk::MarkAllChanged(std::uint64_t version) noexcept
{
	for (auto& entry : changeVersions_)
	{
		entry.second = version;
	}
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

void* Chunk::GetComponentAddress(ComponentTypeId typeId, std::uint32_t row)
{
	if (row >= count_)
	{
		throw std::out_of_range("Chunk row does not contain a live component");
	}
	const auto columnIt = layout_.componentColumns.find(typeId);
	if (columnIt == layout_.componentColumns.end())
	{
		throw std::out_of_range("Chunk does not contain the requested component type");
	}
	return ComponentAddress(storage_, columnIt->second, row);
}
