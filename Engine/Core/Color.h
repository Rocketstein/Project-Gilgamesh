#pragma once

// Color, WIP
struct Color
{
	float r;
	float g;
	float b;
	float a;
	Color() : r(0.0f), g(0.0f), b(0.0f), a(1.0f) {}
	Color(float red, float green, float blue, float alpha = 1.0f)
		: r(red), g(green), b(blue), a(alpha) {}
};