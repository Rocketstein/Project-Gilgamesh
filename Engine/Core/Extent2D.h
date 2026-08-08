#pragma once
#include <cstdint>

struct Extent2D
{
	std::uint32_t width = 0;
	std::uint32_t height = 0;

	bool operator==(const Extent2D&) const = default;
};