#pragma once

// Color, WIP
struct Color4
{
	float r;
	float g;
	float b;
	float a;

	constexpr Color4() : r(0.0f), g(0.0f), b(0.0f), a(1.0f) {}
	constexpr Color4(float red, float green, float blue, float alpha = 1.0f)
		: r(red), g(green), b(blue), a(alpha) {}

	// Contiguous RGBA for graphics APIs that take a float[4].
	constexpr const float* Data() const noexcept { return &r; }
	constexpr float* Data() noexcept { return &r; }
};

static_assert(sizeof(Color4) == 4 * sizeof(float),
	"Color4 must be tightly packed to alias as float[4].");