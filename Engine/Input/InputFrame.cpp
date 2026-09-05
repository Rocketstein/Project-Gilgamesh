#include "InputFrame.h"

namespace
{
template<typename Enum>
constexpr std::size_t ToIndex(Enum value) noexcept
{
    return static_cast<std::size_t>(value);
}
}

bool InputFrame::IsDown(Key key) const noexcept
{
    const std::size_t index = ToIndex(key);
    return index < keysDown_.size() && keysDown_[index] != 0;
}

bool InputFrame::WasPressed(Key key) const noexcept
{
    const std::size_t index = ToIndex(key);
    return index < keysPressed_.size() && keysPressed_[index] != 0;
}

bool InputFrame::WasReleased(Key key) const noexcept
{
    const std::size_t index = ToIndex(key);
    return index < keysReleased_.size() && keysReleased_[index] != 0;
}

bool InputFrame::IsDown(MouseButton button) const noexcept
{
    const std::size_t index = ToIndex(button);
    return index < mouseButtonsDown_.size() &&
        mouseButtonsDown_[index] != 0;
}

bool InputFrame::WasPressed(MouseButton button) const noexcept
{
    const std::size_t index = ToIndex(button);
    return index < mouseButtonsPressed_.size() &&
        mouseButtonsPressed_[index] != 0;
}

bool InputFrame::WasReleased(MouseButton button) const noexcept
{
    const std::size_t index = ToIndex(button);
    return index < mouseButtonsReleased_.size() &&
        mouseButtonsReleased_[index] != 0;
}
