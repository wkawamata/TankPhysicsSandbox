#pragma once

#include "TankTypes.h"

#include <string>

namespace Tank::Physics
{
    struct MortarProfile
    {
        float minimumFireAngleDegrees = 18.0f;
        float maximumAngleDegrees = 40.0f;
        float raiseRateDegreesPerSecond = 12.0f;
        float returnRateDegreesPerSecond = 10.0f;
        float minimumRangeMeters = 8.0f;
        float maximumRangeMeters = 40.0f;
        float minimumAttackRadiusMeters = 2.0f;
        float maximumAttackRadiusMeters = 6.0f;
        float stanceTorqueNm = 500000.0f;
        float stanceDampingNms = 80000.0f;
    };

    MortarProfile ExtractMortarProfile(const TankSettings& settings);
    void ApplyMortarProfile(const MortarProfile& profile, TankSettings& settings);
    std::string SerializeMortarProfile(const MortarProfile& profile);
    bool DeserializeMortarProfile(const std::string& jsonText, MortarProfile& profile,
        std::string* error = nullptr);
}
