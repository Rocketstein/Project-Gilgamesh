#pragma once

#include <cstdint>
#include <limits>

class MeshManager;

// A handle is meaningful only to the MeshManager that created it.
class MeshHandle final
{
public:
	constexpr MeshHandle() noexcept = default;

	[[nodiscard]]
	constexpr bool IsValid() const noexcept
	{
		return index_ != InvalidIndex;
	}

	constexpr explicit operator bool() const noexcept
	{
		return IsValid();
	}

	friend constexpr bool operator==(
		MeshHandle,
		MeshHandle) = default;

private:
	static constexpr std::uint32_t InvalidIndex =
		std::numeric_limits<std::uint32_t>::max();

	constexpr MeshHandle(
		std::uint32_t index,
		std::uint32_t generation) noexcept
		: index_(index), generation_(generation)
	{
	}

	std::uint32_t index_ = InvalidIndex;
	std::uint32_t generation_ = 0;

	friend class MeshManager;
};
