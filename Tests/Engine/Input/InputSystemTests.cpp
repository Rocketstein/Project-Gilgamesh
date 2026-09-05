#include "Engine/Input/InputSystem.h"
#include "Tests/TestFramework.h"

GILGAMESH_TEST("Engine.Input", "TracksHeldAndTransientKeyState")
{
    InputSystem input;

    input.BeginFrame();
    input.SubmitFocus(true);
    input.SubmitKey(Key::W, true);
    input.SubmitKey(Key::W, true); // Autorepeat must not create another edge.
    input.EndFrame();

    GILGAMESH_CHECK_MESSAGE(input.GetFrame().IsDown(Key::W),
        "a pressed key must be held in the frozen frame");
    GILGAMESH_CHECK_MESSAGE(input.GetFrame().WasPressed(Key::W),
        "the initial down transition must be visible for one frame");

    input.BeginFrame();
    input.EndFrame();

    GILGAMESH_CHECK_MESSAGE(input.GetFrame().IsDown(Key::W),
        "held state must persist without another platform event");
    GILGAMESH_CHECK_MESSAGE(!input.GetFrame().WasPressed(Key::W),
        "pressed state must clear at the next frame boundary");

    input.BeginFrame();
    input.SubmitKey(Key::W, false);
    input.EndFrame();

    GILGAMESH_CHECK_MESSAGE(!input.GetFrame().IsDown(Key::W),
        "a released key must no longer be held");
    GILGAMESH_CHECK_MESSAGE(input.GetFrame().WasReleased(Key::W),
        "the up transition must be visible for one frame");
}

GILGAMESH_TEST("Engine.Input", "PreservesPressAndReleaseWithinOneFrame")
{
    InputSystem input;

    input.BeginFrame();
    input.SubmitKey(Key::Escape, true);
    input.SubmitKey(Key::Escape, false);
    input.EndFrame();

    GILGAMESH_CHECK_MESSAGE(!input.GetFrame().IsDown(Key::Escape),
        "a key released in the same pump must finish up");
    GILGAMESH_CHECK_MESSAGE(input.GetFrame().WasPressed(Key::Escape),
        "a short tap must preserve its press edge");
    GILGAMESH_CHECK_MESSAGE(input.GetFrame().WasReleased(Key::Escape),
        "a short tap must preserve its release edge");
}

GILGAMESH_TEST("Engine.Input", "AccumulatesFrameMouseInput")
{
    InputSystem input;

    input.BeginFrame();
    input.SubmitMouseMove({ 10.0f, 20.0f });
    input.SubmitMouseMove({ 13.0f, 24.0f });
    input.SubmitMouseDelta({ 2.0f, -1.0f });
    input.SubmitMouseWheel(1.0f);
    input.SubmitMouseWheel(-0.5f);
    input.EndFrame();

    GILGAMESH_CHECK_MESSAGE(
        (input.GetFrame().GetMousePosition() == Vector2{ 13.0f, 24.0f }),
        "the frame must expose the latest absolute cursor position");
    GILGAMESH_CHECK_MESSAGE(
        (input.GetFrame().GetMouseDelta() == Vector2{ 5.0f, 3.0f }),
        "relative mouse sources must accumulate during the message pump");
    GILGAMESH_CHECK_MESSAGE(input.GetFrame().GetWheelDelta() == 0.5f,
        "wheel movement must accumulate during the message pump");

    input.BeginFrame();
    input.EndFrame();

    GILGAMESH_CHECK_MESSAGE(input.GetFrame().GetMouseDelta() == Vector2{},
        "mouse delta must clear at the next frame boundary");
    GILGAMESH_CHECK_MESSAGE(input.GetFrame().GetWheelDelta() == 0.0f,
        "wheel delta must clear at the next frame boundary");
}

GILGAMESH_TEST("Engine.Input", "ReleasesHeldInputOnFocusLoss")
{
    InputSystem input;

    input.BeginFrame();
    input.SubmitFocus(true);
    input.SubmitKey(Key::A, true);
    input.SubmitMouseButton(MouseButton::Right, true);
    input.EndFrame();

    input.BeginFrame();
    input.SubmitFocus(false);
    input.EndFrame();

    const InputFrame& frame = input.GetFrame();
    GILGAMESH_CHECK_MESSAGE(!frame.HasFocus(),
        "focus state must follow the platform notification");
    GILGAMESH_CHECK_MESSAGE(!frame.IsDown(Key::A) && frame.WasReleased(Key::A),
        "focus loss must release held keys");
    GILGAMESH_CHECK_MESSAGE(
        !frame.IsDown(MouseButton::Right) &&
        frame.WasReleased(MouseButton::Right),
        "focus loss must release held mouse buttons");
}
