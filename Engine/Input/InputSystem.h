#pragma once

#include "Engine/Input/InputFrame.h"

// Collects platform-neutral physical device state. Platform backends submit
// changes between BeginFrame and EndFrame; program code reads only GetFrame().
class InputSystem
{
public:
    void Reset() noexcept;
    void BeginFrame() noexcept;
    void EndFrame() noexcept;

    [[nodiscard]] const InputFrame& GetFrame() const noexcept { return frame_; }

    // Backend-facing input sink. These operations deliberately describe
    // physical state rather than editor or gameplay actions.
    void SubmitKey(Key key, bool isDown) noexcept;
    void SubmitMouseButton(MouseButton button, bool isDown) noexcept;
    void SubmitMouseMove(Vector2 position) noexcept;
    void SubmitMouseDelta(Vector2 delta) noexcept;
    void SubmitMouseWheel(float delta) noexcept;
    void SubmitFocus(bool hasFocus) noexcept;

private:
    void ReleaseHeldInputs() noexcept;

    InputFrame frame_{};
    InputFrame pendingFrame_{};
    bool hasMousePosition_ = false;
};
