#include "Physics/TankSettingsJson.h"

#include <cmath>
#include <iostream>

namespace
{
    bool NearlyEqual(float left, float right)
    {
        return std::abs(left - right) < 0.0001f;
    }

    bool Check(bool condition, const char* message)
    {
        if (!condition)
        {
            std::cerr << "FAIL TankSettings JSON: " << message << "\n";
        }
        return condition;
    }
}

int main()
{
    Tank::Physics::TankSettings source;
    source.chassisMassKg = 5200.0f;
    source.rollTorqueNm = 175000.0f;
    source.rollDistanceM = 3.0f;
    source.rollTorqueCutoffDegrees = 80.0f;
    source.rollStabilizationTorqueNm = 45000.0f;
    source.rollStabilizationDampingNms = 14000.0f;
    source.rideHeightScale = 0.75f;
    source.startUpsideDown = true;

    Tank::Physics::TankSettings loaded;
    std::string error;
    bool passed = true;
    passed &= Check(
        Tank::Physics::DeserializeTankSettings(
            Tank::Physics::SerializeTankSettings(source),
            loaded,
            &error),
        "serialized settings must deserialize");
    passed &= Check(NearlyEqual(loaded.chassisMassKg, source.chassisMassKg),
        "chassis mass must round trip");
    passed &= Check(NearlyEqual(loaded.rollTorqueNm, source.rollTorqueNm),
        "roll torque must round trip");
    passed &= Check(NearlyEqual(loaded.rollDistanceM, source.rollDistanceM),
        "roll distance must round trip");
    passed &= Check(
        NearlyEqual(loaded.rollTorqueCutoffDegrees, source.rollTorqueCutoffDegrees),
        "roll torque cutoff must round trip");
    passed &= Check(
        NearlyEqual(loaded.rollStabilizationTorqueNm, source.rollStabilizationTorqueNm),
        "roll stabilization torque must round trip");
    passed &= Check(
        NearlyEqual(loaded.rollStabilizationDampingNms, source.rollStabilizationDampingNms),
        "roll stabilization damping must round trip");
    passed &= Check(NearlyEqual(loaded.rideHeightScale, source.rideHeightScale),
        "ride height must round trip");
    passed &= Check(loaded.startUpsideDown == source.startUpsideDown,
        "start orientation must round trip");

    const Tank::Physics::TankSettings beforeInvalid = loaded;
    passed &= Check(
        !Tank::Physics::DeserializeTankSettings("{invalid", loaded, &error),
        "invalid JSON must fail");
    passed &= Check(NearlyEqual(loaded.chassisMassKg, beforeInvalid.chassisMassKg),
        "invalid JSON must not modify settings");

    passed &= Check(
        Tank::Physics::DeserializeTankSettings(
            R"({"version":1,"chassisMassKg":6100.0})",
            loaded,
            &error),
        "missing fields must use current values");
    passed &= Check(NearlyEqual(loaded.chassisMassKg, 6100.0f),
        "present field must load");
    passed &= Check(NearlyEqual(loaded.rollTorqueNm, beforeInvalid.rollTorqueNm),
        "missing field must preserve current value");

    if (!passed)
    {
        return 1;
    }

    std::cout << "PASS TankSettings JSON\n";
    return 0;
}
