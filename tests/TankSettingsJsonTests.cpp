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
    source.rollingInputEnabled = false;
    source.rollTorqueNm = 175000.0f;
    source.rollDistanceM = 3.0f;
    source.rollTorqueCutoffDegrees = 80.0f;
    source.rollStabilizationTorqueNm = 45000.0f;
    source.rollStabilizationDampingNms = 14000.0f;
    source.trackWidthM = 0.42f;
    source.trackSpacingM = 2.75f;
    source.chassisWidthM = 2.65f;
    source.chassisLengthM = 4.75f;
    source.endWheelRadiusM = 0.44f;
    source.roadWheelRadiusM = 0.32f;
    source.roadWheelCount = 4;
    source.endWheelOffsetM = 0.35f;
    source.twoRoadWheelOffsetM = 0.82f;
    source.threeRoadWheelOffsetM = 1.15f;
    source.rideHeightScale = 0.75f;
    source.neutralBrakeEnabled = false;
    source.neutralBrakeAmount = 0.35f;
    source.stationaryTurnInnerTrackRatio = 0.25f;
    source.stationaryTurnLeftTraction = 0.6f;
    source.stationaryTurnRightTraction = 0.7f;
    source.pivotTurnLeftTraction = 0.8f;
    source.pivotTurnRightTraction = 0.9f;
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
    passed &= Check(loaded.rollingInputEnabled == source.rollingInputEnabled,
        "rolling input enabled must round trip");
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
    passed &= Check(NearlyEqual(loaded.trackWidthM, source.trackWidthM),
        "track width must round trip");
    passed &= Check(NearlyEqual(loaded.trackSpacingM, source.trackSpacingM),
        "track spacing must round trip");
    passed &= Check(NearlyEqual(loaded.chassisWidthM, source.chassisWidthM),
        "chassis width must round trip");
    passed &= Check(NearlyEqual(loaded.chassisLengthM, source.chassisLengthM),
        "chassis length must round trip");
    passed &= Check(NearlyEqual(loaded.endWheelRadiusM, source.endWheelRadiusM),
        "end wheel radius must round trip");
    passed &= Check(NearlyEqual(loaded.roadWheelRadiusM, source.roadWheelRadiusM),
        "road wheel radius must round trip");
    passed &= Check(loaded.roadWheelCount == source.roadWheelCount,
        "road wheel count must round trip");
    passed &= Check(NearlyEqual(loaded.endWheelOffsetM, source.endWheelOffsetM),
        "end wheel offset must round trip");
    passed &= Check(NearlyEqual(loaded.twoRoadWheelOffsetM, source.twoRoadWheelOffsetM),
        "two road wheel offset must round trip");
    passed &= Check(NearlyEqual(loaded.threeRoadWheelOffsetM, source.threeRoadWheelOffsetM),
        "three road wheel offset must round trip");
    passed &= Check(NearlyEqual(loaded.rideHeightScale, source.rideHeightScale),
        "ride height must round trip");
    passed &= Check(loaded.neutralBrakeEnabled == source.neutralBrakeEnabled,
        "neutral brake enabled must round trip");
    passed &= Check(NearlyEqual(loaded.neutralBrakeAmount, source.neutralBrakeAmount),
        "neutral brake amount must round trip");
    passed &= Check(
        NearlyEqual(
            loaded.stationaryTurnInnerTrackRatio,
            source.stationaryTurnInnerTrackRatio),
        "stationary turn inner track ratio must round trip");
    passed &= Check(
        NearlyEqual(
            loaded.stationaryTurnLeftTraction,
            source.stationaryTurnLeftTraction),
        "stationary turn left traction must round trip");
    passed &= Check(
        NearlyEqual(
            loaded.stationaryTurnRightTraction,
            source.stationaryTurnRightTraction),
        "stationary turn right traction must round trip");
    passed &= Check(
        NearlyEqual(loaded.pivotTurnLeftTraction, source.pivotTurnLeftTraction),
        "pivot turn left traction must round trip");
    passed &= Check(
        NearlyEqual(loaded.pivotTurnRightTraction, source.pivotTurnRightTraction),
        "pivot turn right traction must round trip");
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

    Tank::Physics::TankSettings legacyLoaded;
    passed &= Check(
        Tank::Physics::DeserializeTankSettings(
            R"({"version":1,"wheelRadiusM":0.36})",
            legacyLoaded,
            &error),
        "version 1 wheel radius must migrate");
    passed &= Check(
        NearlyEqual(legacyLoaded.endWheelRadiusM, 0.36f) &&
            NearlyEqual(legacyLoaded.roadWheelRadiusM, 0.36f),
        "legacy wheel radius must initialize both wheel groups");

    const Tank::Physics::TankSettings beforeFutureVersion = loaded;
    passed &= Check(
        !Tank::Physics::DeserializeTankSettings(
            R"({"version":999,"chassisMassKg":1.0})",
            loaded,
            &error),
        "future schema version must fail");
    passed &= Check(
        NearlyEqual(loaded.chassisMassKg, beforeFutureVersion.chassisMassKg),
        "unsupported version must not modify settings");

    if (!passed)
    {
        return 1;
    }

    std::cout << "PASS TankSettings JSON\n";
    return 0;
}
