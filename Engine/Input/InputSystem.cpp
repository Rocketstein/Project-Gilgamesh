#include "InputSystem.h"

#include <cstddef>

namespace
{
template<typename Enum>
constexpr std::size_t ToIndex(Enum value) noexcept
{
    return static_cast<std::size_t>(value);
}
}

void InputSystem::Reset() noexcept
{
    frame_ = {};
    pendingFrame_ = {};
    hasMousePosition_ = false;
}

void InputSystem::BeginFrame() noexcept
{
    pendingFrame_ = frame_;
    pendingFrame_.keysPressed_.fill(0);
    pendingFrame_.keysReleased_.fill(0);
    pendingFrame_.mouseButtonsPressed_.fill(0);
    pendingFrame_.mouseButtonsReleased_.fill(0);
    pendingFrame_.mouseDelta_ = {};
    pendingFrame_.wheelDelta_ = 0.0f;
}

void InputSystem::EndFrame() noexcept
{
    frame_ = pendingFrame_;
}

void InputSystem::SubmitKey(Key key, bool isDown) noexcept
{
    const std::size_t index = ToIndex(key);
    if (key == Key::Unknown || index >= pendingFrame_.keysDown_.size())
        return;

    const bool wasDown = pendingFrame_.keysDown_[index] != 0;
    if (wasDown == isDown)
        return;

    pendingFrame_.keysDown_[index] = isDown ? 1 : 0;
    if (isDown)
        pendingFrame_.keysPressed_[index] = 1;
    else
        pendingFrame_.keysReleased_[index] = 1;
}

void InputSystem::SubmitMouseButton(
    MouseButton button,
    bool isDown) noexcept
{
    const std::size_t index = ToIndex(button);
    if (index >= pendingFrame_.mouseButtonsDown_.size())
        return;

    const bool wasDown = pendingFrame_.mouseButtonsDown_[index] != 0;
    if (wasDown == isDown)
        return;

    pendingFrame_.mouseButtonsDown_[index] = isDown ? 1 : 0;
    if (isDown)
        pendingFrame_.mouseButtonsPressed_[index] = 1;
    else
        pendingFrame_.mouseButtonsReleased_[index] = 1;
}

void InputSystem::SubmitMouseMove(Vector2 position) noexcept
{
    if (hasMousePosition_)
        pendingFrame_.mouseDelta_ += position - pendingFrame_.mousePosition_;

    pendingFrame_.mousePosition_ = position;
    hasMousePosition_ = true;
}

void InputSystem::SubmitMouseDelta(Vector2 delta) noexcept
{
    pendingFrame_.mouseDelta_ += delta;
}

void InputSystem::SubmitMouseWheel(float delta) noexcept
{
    pendingFrame_.wheelDelta_ += delta;
}

void InputSystem::SubmitFocus(bool hasFocus) noexcept
{
    if (pendingFrame_.hasFocus_ == hasFocus)
        return;

    pendingFrame_.hasFocus_ = hasFocus;
    if (!hasFocus)
    {
        ReleaseHeldInputs();
        pendingFrame_.mouseDelta_ = {};
        pendingFrame_.wheelDelta_ = 0.0f;
        hasMousePosition_ = false;
    }
}

void InputSystem::ReleaseHeldInputs() noexcept
{
    for (std::size_t index = 0;
        index < pendingFrame_.keysDown_.size();
        ++index)
    {
        if (pendingFrame_.keysDown_[index] != 0)
        {
            pendingFrame_.keysDown_[index] = 0;
            pendingFrame_.keysReleased_[index] = 1;
        }
    }

    for (std::size_t index = 0;
        index < pendingFrame_.mouseButtonsDown_.size();
        ++index)
    {
        if (pendingFrame_.mouseButtonsDown_[index] != 0)
        {
            pendingFrame_.mouseButtonsDown_[index] = 0;
            pendingFrame_.mouseButtonsReleased_[index] = 1;
        }
    }
}
