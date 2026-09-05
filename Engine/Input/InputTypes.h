#pragma once

#include <cstdint>

enum class Key : std::uint16_t
{
    Unknown = 0,

    A, B, C, D, E, F, G, H, I, J, K, L, M,
    N, O, P, Q, R, S, T, U, V, W, X, Y, Z,

    Digit0, Digit1, Digit2, Digit3, Digit4,
    Digit5, Digit6, Digit7, Digit8, Digit9,

    Escape,
    Space,
    Enter,
    Tab,
    Backspace,
    Insert,
    Delete,
    Home,
    End,
    PageUp,
    PageDown,
    ArrowUp,
    ArrowDown,
    ArrowLeft,
    ArrowRight,
    LeftShift,
    RightShift,
    LeftControl,
    RightControl,
    LeftAlt,
    RightAlt,
    CapsLock,
    F1, F2, F3, F4, F5, F6,
    F7, F8, F9, F10, F11, F12,

    Count
};

enum class MouseButton : std::uint8_t
{
    Left = 0,
    Right,
    Middle,
    X1,
    X2,

    Count
};
