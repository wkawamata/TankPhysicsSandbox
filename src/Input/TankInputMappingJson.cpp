#include "TankInputMappingJson.h"

namespace Tank::Input
{
    TankInputMappingSettings LoadTankInputMappingSettings(
        const nlohmann::json& json)
    {
        TankInputMappingSettings settings;
        settings.stickDeadzone = json.value("stickDeadzone", settings.stickDeadzone);
        settings.pivotThreshold = json.value("pivotThreshold", settings.pivotThreshold);
        settings.pivotSpeed = json.value("pivotSpeed", settings.pivotSpeed);
        settings.steeringTrackReduction = json.value("steeringTrackReduction", settings.steeringTrackReduction);
        settings.leftLeverAxis = json.value("leftLeverAxis", settings.leftLeverAxis);
        settings.rightLeverAxis = json.value("rightLeverAxis", settings.rightLeverAxis);
        return settings;
    }

    nlohmann::json SaveTankInputMappingSettings(
        const TankInputMappingSettings& settings)
    {
        return {{"stickDeadzone", settings.stickDeadzone},
            {"pivotThreshold", settings.pivotThreshold},
            {"pivotSpeed", settings.pivotSpeed},
            {"steeringTrackReduction", settings.steeringTrackReduction},
            {"leftLeverAxis", settings.leftLeverAxis},
            {"rightLeverAxis", settings.rightLeverAxis}};
    }
}
