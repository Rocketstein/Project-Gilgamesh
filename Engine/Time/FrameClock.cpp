#include "FrameClock.h"

void FrameClock::Reset() noexcept
{
	previousTime_ = Clock::now();
	initialized_ = true;
}

FrameContext FrameClock::BeginFrame() noexcept
{
    const auto now = Clock::now();

    if (!initialized_)
    {
        previousTime_ = now;
        initialized_ = true;

        return {
            .realDeltaTime = 0.0f,
            .frameNumber = frameNumber_++
        };
    }

    const float elapsed =
        std::chrono::duration<float>(
            now - previousTime_).count();

    previousTime_ = now;

    // Prevent a breakpoint or long OS stall from advancing
    // frame-driven systems by several seconds at once.
    constexpr float maximumDelta = 0.25f;

    return {
        .realDeltaTime =
            std::clamp(elapsed, 0.0f, maximumDelta),
        .frameNumber = frameNumber_++
    };
}