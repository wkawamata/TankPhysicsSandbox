#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

namespace Tank::Input
{
    struct GamepadState
    {
        static constexpr std::size_t MaxRawAxes = 16;
        static constexpr std::size_t MaxRawButtons = 64;
        static constexpr std::size_t MaxRawSwitches = 8;
        static constexpr std::uint32_t BrakeButtonIndex = 3;

        bool connected = false;
        bool hasGamepadMapping = false;
        std::string deviceName;
        std::uint32_t buttonCount = 0;
        std::uint32_t axisCount = 0;
        std::uint32_t switchCount = 0;
        std::uint16_t vendorId = 0;
        std::uint16_t productId = 0;
        std::array<float, MaxRawAxes> rawAxes = {};
        std::array<bool, MaxRawButtons> rawButtons = {};
        std::array<std::uint32_t, MaxRawSwitches> rawSwitches = {};
        float leftStickX = 0.0f;
        float leftStickY = 0.0f;
        bool brakePressed = false;
    };
}
