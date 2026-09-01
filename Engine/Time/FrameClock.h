#pragma once
#include <algorithm>
#include <chrono>

#include "Engine/Runtime/FrameContext.h"

class FrameClock
{
public:
	using Clock = std::chrono::steady_clock;

	void Reset() noexcept;
	FrameContext BeginFrame() noexcept;

private:
	Clock::time_point previousTime_{};
	std::uint64_t frameNumber_ = 0;
	bool initialized_ = false;
};