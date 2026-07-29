#include "Physics/TrackedVehicleTest.h"

#include <cmath>
#include <iostream>

namespace
{
    bool IsFinite(float v)
    {
        return std::isfinite(v);
    }

    bool Check(bool condition, const char* message)
    {
        if (!condition)
        {
            std::cerr << "FAIL TrackedVehicle: " << message << "\n";
        }

        return condition;
    }
}

int main()
{
    bool passed = true;

    Tank::Physics::TrackedVehicleTest test;
    Tank::Physics::TankSettings settings;
    settings.chassisMassKg = 5500.0f;
    test.Initialize(settings);
    passed &= Check(std::abs(test.Settings().chassisMassKg - 5500.0f) < 0.001f,
        "configured chassis mass must be retained");

    const int numSteps = 300;
    const float dt = 1.0f / 60.0f;

    for (int i = 0; i < numSteps; ++i)
    {
        test.Step(dt);
    }

    const Tank::Physics::TrackedVehicleTestState& state = test.State();

    passed &= Check(IsFinite(state.bodyPosition.x), "position X must be finite");
    passed &= Check(IsFinite(state.bodyPosition.y), "position Y must be finite");
    passed &= Check(IsFinite(state.bodyPosition.z), "position Z must be finite");

    passed &= Check(IsFinite(state.bodyRotation.x), "rotation X must be finite");
    passed &= Check(IsFinite(state.bodyRotation.y), "rotation Y must be finite");
    passed &= Check(IsFinite(state.bodyRotation.z), "rotation Z must be finite");
    passed &= Check(IsFinite(state.bodyRotation.w), "rotation W must be finite");

    passed &= Check(state.stepIndex == numSteps, "step index must equal numSteps");

    passed &= Check(state.bodyPosition.y > 0.1f,
        "body must settle above the floor");

    passed &= Check(state.bodyPosition.y < 5.0f,
        "body must not fly away");

    passed &= Check(std::abs(state.bodyPosition.x) < 0.25f,
        "neutral body must not drift sideways");
    passed &= Check(std::abs(state.bodyPosition.z) < 0.25f,
        "neutral body must not drift forward or backward");
    passed &= Check(std::abs(state.bodyRotation.x) < 0.1f,
        "neutral body roll must remain small");
    passed &= Check(std::abs(state.bodyRotation.z) < 0.1f,
        "neutral body pitch must remain small");

    const float linearSpeedSquared =
        state.linearVelocity.x * state.linearVelocity.x +
        state.linearVelocity.y * state.linearVelocity.y +
        state.linearVelocity.z * state.linearVelocity.z;
    const float angularSpeedSquared =
        state.angularVelocity.x * state.angularVelocity.x +
        state.angularVelocity.y * state.angularVelocity.y +
        state.angularVelocity.z * state.angularVelocity.z;
    passed &= Check(linearSpeedSquared < 0.04f,
        "neutral body linear speed must settle below 0.2 m/s");
    passed &= Check(angularSpeedSquared < 0.04f,
        "neutral body angular speed must settle below 0.2 rad/s");

    const int wheelsPerSurface = settings.roadWheelCount + 2;
    const int wheelsPerTrack = wheelsPerSurface * Tank::Physics::kTankSurfacesPerTrack;
    const int expectedWheelCount = wheelsPerTrack * Tank::Physics::kTankTrackCount;
    passed &= Check(state.wheelCount == expectedWheelCount,
        "tracked vehicle must expose all ten wheel snapshots");

    int contactCount = 0;
    for (int i = 0; i < state.wheelCount; ++i)
    {
        const Tank::Physics::TrackedWheelState& wheel = state.wheels[static_cast<size_t>(i)];
        passed &= Check(wheel.trackIndex == i / wheelsPerTrack,
            "wheel track index must match snapshot order");
        passed &= Check(wheel.wheelIndex == i % wheelsPerTrack,
            "wheel index must match snapshot order");
        passed &= Check(
            wheel.upperSurface ==
                (wheel.wheelIndex >= wheelsPerSurface),
            "wheel surface flag must match snapshot order");
        passed &= Check(IsFinite(wheel.transform.position.x), "wheel position X must be finite");
        passed &= Check(IsFinite(wheel.transform.position.y), "wheel position Y must be finite");
        passed &= Check(IsFinite(wheel.transform.position.z), "wheel position Z must be finite");
        passed &= Check(IsFinite(wheel.transform.rotation.x), "wheel rotation X must be finite");
        passed &= Check(IsFinite(wheel.transform.rotation.y), "wheel rotation Y must be finite");
        passed &= Check(IsFinite(wheel.transform.rotation.z), "wheel rotation Z must be finite");
        passed &= Check(IsFinite(wheel.transform.rotation.w), "wheel rotation W must be finite");
        passed &= Check(IsFinite(wheel.suspensionOrigin.x), "suspension origin X must be finite");
        passed &= Check(IsFinite(wheel.suspensionOrigin.y), "suspension origin Y must be finite");
        passed &= Check(IsFinite(wheel.suspensionOrigin.z), "suspension origin Z must be finite");
        passed &= Check(IsFinite(wheel.suspensionDirection.x),
            "suspension direction X must be finite");
        passed &= Check(IsFinite(wheel.suspensionDirection.y),
            "suspension direction Y must be finite");
        passed &= Check(IsFinite(wheel.suspensionDirection.z),
            "suspension direction Z must be finite");
        passed &= Check(IsFinite(wheel.suspensionLength), "suspension length must be finite");
        passed &= Check(wheel.suspensionLength >= 0.0f && wheel.suspensionLength <= 0.5f,
            "suspension length must remain within the configured range");
        if (wheel.hasContact)
        {
            passed &= Check(IsFinite(wheel.contactPosition.x), "contact position X must be finite");
            passed &= Check(IsFinite(wheel.contactPosition.y), "contact position Y must be finite");
            passed &= Check(IsFinite(wheel.contactPosition.z), "contact position Z must be finite");
            passed &= Check(IsFinite(wheel.contactNormal.x), "contact normal X must be finite");
            passed &= Check(IsFinite(wheel.contactNormal.y), "contact normal Y must be finite");
            passed &= Check(IsFinite(wheel.contactNormal.z), "contact normal Z must be finite");
            const float normalLengthSquared =
                wheel.contactNormal.x * wheel.contactNormal.x +
                wheel.contactNormal.y * wheel.contactNormal.y +
                wheel.contactNormal.z * wheel.contactNormal.z;
            passed &= Check(
                std::abs(normalLengthSquared - 1.0f) < 0.01f,
                "contact normal must be normalized");
            ++contactCount;
        }
    }
    passed &= Check(contactCount > 0, "at least one wheel must contact the floor");

    for (int roadWheelCount = 2; roadWheelCount <= 4; ++roadWheelCount)
    {
        Tank::Physics::TankSettings layoutSettings;
        layoutSettings.roadWheelCount = roadWheelCount;
        layoutSettings.endWheelOffsetM = 0.4f;
        if (roadWheelCount == 2)
        {
            layoutSettings.twoRoadWheelOffsetM = 1.1f;
        }
        else if (roadWheelCount == 3)
        {
            layoutSettings.threeRoadWheelOffsetM = 1.25f;
        }
        Tank::Physics::TrackedVehicleTest layoutTest;
        layoutTest.Initialize(layoutSettings);
        layoutTest.Step(dt);
        const int expectedLayoutWheelCount =
            Tank::Physics::kTankTrackCount *
            Tank::Physics::kTankSurfacesPerTrack *
            (roadWheelCount + 2);
        passed &= Check(
            layoutTest.State().wheelCount == expectedLayoutWheelCount,
            "selected road wheel layout must set the physics wheel count");
        const int lastLowerWheel = roadWheelCount + 1;
        passed &= Check(
            std::abs(layoutTest.State().wheels[0].transform.position.z - 1.6f) < 0.01f,
            "front end wheel must use the configured inward offset");
        passed &= Check(
            std::abs(layoutTest.State().wheels[static_cast<size_t>(lastLowerWheel)]
                    .transform.position.z + 1.6f) < 0.01f,
            "rear end wheel must use the configured inward offset");
        if (roadWheelCount == 2)
        {
            const float frontMiddleZ = layoutTest.State().wheels[1].transform.position.z;
            const float rearMiddleZ = layoutTest.State().wheels[2].transform.position.z;
            passed &= Check(std::abs(frontMiddleZ - 1.1f) < 0.01f,
                "front middle wheel must use the configured positive offset");
            passed &= Check(std::abs(rearMiddleZ + 1.1f) < 0.01f,
                "rear middle wheel must use the configured negative offset");
        }
        else if (roadWheelCount == 3)
        {
            const float frontMiddleZ = layoutTest.State().wheels[1].transform.position.z;
            const float centerMiddleZ = layoutTest.State().wheels[2].transform.position.z;
            const float rearMiddleZ = layoutTest.State().wheels[3].transform.position.z;
            passed &= Check(std::abs(frontMiddleZ - 1.25f) < 0.01f,
                "front middle wheel must use the configured positive offset");
            passed &= Check(std::abs(centerMiddleZ) < 0.01f,
                "center middle wheel must remain centered");
            passed &= Check(std::abs(rearMiddleZ + 1.25f) < 0.01f,
                "rear middle wheel must use the configured negative offset");
        }
    }

    if (!passed)
    {
        std::cerr << "  bodyPosition: (" << state.bodyPosition.x << ", "
            << state.bodyPosition.y << ", " << state.bodyPosition.z << ")\n";
        std::cerr << "  bodyRotation: (" << state.bodyRotation.x << ", "
            << state.bodyRotation.y << ", " << state.bodyRotation.z << ", "
            << state.bodyRotation.w << ")\n";
        return 1;
    }

    std::cout << "PASS TrackedVehicle neutral\n";
    return 0;
}
