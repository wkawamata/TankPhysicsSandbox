#pragma once

#include "GamepadState.h"
#include "Physics/TankTypes.h"

namespace Tank::Input
{
    // Explicit keyboard rolling overrides the pad's horizontal lever pair.
    // With neither key (or both keys) held, retain the analog pair.
    inline void ApplyKeyboardRollingInput(
        Physics::TankInput& input, bool rollLeft, bool rollRight)
    {
        if (rollLeft != rollRight)
        {
            const float sign = rollLeft ? -1.0f : 1.0f;
            input.leftLeverX = sign;
            input.rightLeverX = sign;
        }
    }

    struct TankInputMappingSettings
    {
        float stickDeadzone = 0.15f;
        float pivotThreshold = 0.1f;
        float pivotSpeed = 1.0f;
        float steeringTrackReduction = 0.4f;
        std::size_t leftLeverAxis = 2;
        std::size_t rightLeverAxis = 3;
    };

    Physics::TankInput MapGamepadToTankInput(
        const GamepadState& state,
        const TankInputMappingSettings& settings = {});

}
