#pragma once

#include "GamepadState.h"
#include "Physics/TankTypes.h"

namespace Tank::Input
{
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
