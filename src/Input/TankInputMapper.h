#pragma once

#include "GamepadState.h"
#include "Physics/TankTypes.h"
#include <nlohmann/json.hpp>

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

    inline void to_json(nlohmann::json& json, const TankInputMappingSettings& s)
    {
        json = {{"stickDeadzone", s.stickDeadzone},
            {"pivotThreshold", s.pivotThreshold},
            {"pivotSpeed", s.pivotSpeed},
            {"steeringTrackReduction", s.steeringTrackReduction},
            {"leftLeverAxis", s.leftLeverAxis},
            {"rightLeverAxis", s.rightLeverAxis}};
    }

    inline void from_json(const nlohmann::json& json, TankInputMappingSettings& s)
    {
        s.stickDeadzone = json.value("stickDeadzone", s.stickDeadzone);
        s.pivotThreshold = json.value("pivotThreshold", s.pivotThreshold);
        s.pivotSpeed = json.value("pivotSpeed", s.pivotSpeed);
        s.steeringTrackReduction = json.value("steeringTrackReduction", s.steeringTrackReduction);
        s.leftLeverAxis = json.value("leftLeverAxis", s.leftLeverAxis);
        s.rightLeverAxis = json.value("rightLeverAxis", s.rightLeverAxis);
    }
}
