#pragma once

namespace Tank::Input
{
    struct GamepadState
    {
        bool connected = false;
        float leftStickX = 0.0f;
        float leftStickY = 0.0f;
        bool brakePressed = false;
    };
}
