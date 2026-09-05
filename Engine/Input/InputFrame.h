#pragma once

#include "Engine/Core/Math/Vector.h"
#include "Engine/Input/InputTypes.h"

#include <array>
#include <cstddef>
#include <cstdint>

class InputSystem;

// Immutable-by-convention snapshot exposed to program and controller code.
// InputSystem is the only type allowed to mutate its state.
class InputFrame
{
public:
    [[nodiscard]] bool IsDown(Key key) const noexcept;
    [[nodiscard]] bool WasPressed(Key key) const noexcept;
    [[nodiscard]] bool WasReleased(Key key) const noexcept;

    [[nodiscard]] bool IsDown(MouseButton button) const noexcept;
    [[nodiscard]] bool WasPressed(MouseButton button) const noexcept;
    [[nodiscard]] bool WasReleased(MouseButton button) const noexcept;

    [[nodiscard]] Vector2 GetMousePosition() const noexcept { return mousePosition_; }
    [[nodiscard]] Vector2 GetMouseDelta() const noexcept { return mouseDelta_; }
    [[nodiscard]] float GetWheelDelta() const noexcept { return wheelDelta_; }
    [[nodiscard]] bool HasFocus() const noexcept { return hasFocus_; }

private:
    static constexpr std::size_t KeyCount = static_cast<std::size_t>(Key::Count);
    static constexpr std::size_t MouseButtonCount =
        static_cast<std::size_t>(MouseButton::Count);

    using KeyState = std::array<std::uint8_t, KeyCount>;
    using MouseButtonState = std::array<std::uint8_t, MouseButtonCount>;

    KeyState keysDown_{};
    KeyState keysPressed_{};
    KeyState keysReleased_{};

    MouseButtonState mouseButtonsDown_{};
    MouseButtonState mouseButtonsPressed_{};
    MouseButtonState mouseButtonsReleased_{};

    Vector2 mousePosition_{};
    Vector2 mouseDelta_{};
    float wheelDelta_ = 0.0f;
    bool hasFocus_ = false;

    friend class InputSystem;
};
