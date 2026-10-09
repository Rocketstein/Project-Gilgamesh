#pragma once

#include <cstdint>
#include <limits>

// Identifies one entity during the lifetime of its owning registry.
struct Entity final
{
	constexpr Entity() noexcept = default;	

	[[nodiscard]]
	constexpr bool IsNull() const noexcept { return index_ == InvalidIndex; }

	friend constexpr bool operator==(Entity, Entity) noexcept = default;
	friend constexpr auto operator<=>(Entity, Entity) noexcept = default;

private:
	static constexpr std::uint32_t InvalidIndex = 
		std::numeric_limits<std::uint32_t>::max();

	constexpr Entity(
		std::uint32_t index,
		std::uint32_t generation,
		std::uint64_t ownerId,
		std::uint32_t owningWorldID) noexcept
		: index_(index), generation_(generation), ownerId_(ownerId),
		  owningWorldID_(owningWorldID) {}

	std::uint32_t index_	  = InvalidIndex;
	std::uint32_t generation_ = 0;
	std::uint64_t ownerId_	  = 0;
	std::uint32_t owningWorldID_ = 0; // World identity supplied by the issuing registry.

	friend class EntityRegistry;
};
