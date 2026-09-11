#pragma once

#include "TankTypes.h"

#include <string>

namespace Tank::Physics
{
    // A portable tuning preset for rolling only. It intentionally excludes
    // geometry, suspension, track, engine, and other vehicle physics fields.
    struct RollingProfile
    {
        bool inputEnabled = true;
        float speedMultiplier = 1.0f;
        float torqueNm = 200000.0f;
        float returnDecisionDegrees = 75.0f;
        float approachStartDegrees = 80.0f;
        float approachDampingNms = 30000.0f;
        float commitTorqueNm = 100000.0f;
        float airBrakeTorqueNm = 150000.0f;
        float airBrakeReleaseDegrees = 30.0f;
        bool distanceMatchesVehicleWidth = true;
        float travelVehicleWidths = 1.0f;
        float distanceM = 2.4f;
        float torqueCutoffDegrees = 90.0f;
        float stabilizationTorqueNm = 30000.0f;
        float stabilizationDampingNms = 10000.0f;
    };

    RollingProfile ExtractRollingProfile(const TankSettings& settings);
    void ApplyRollingProfile(const RollingProfile& profile, TankSettings& settings);

    std::string SerializeRollingProfile(const RollingProfile& profile);
    bool DeserializeRollingProfile(
        const std::string& jsonText,
        RollingProfile& profile,
        std::string* error = nullptr);
}
