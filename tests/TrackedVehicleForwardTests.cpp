#include "Physics/TrackedVehicleTest.h"

#include <cmath>
#include <iostream>

namespace
{
    bool Check(bool condition, const char* message)
    {
        if (!condition)
        {
            std::cerr << "FAIL TrackedVehicle forward: " << message << "\n";
        }

        return condition;
    }
}

int main()
{
    constexpr float dt = 1.0f / 60.0f;

    Tank::Physics::TrackedVehicleTest test;
    test.Initialize();

    for (int i = 0; i < 180; ++i)
    {
        test.Step(dt);
    }

    const Tank::Physics::Vec3 startPosition = test.State().bodyPosition;
    const Tank::Physics::Quat startWheelRotation =
        test.State().wheels[0].transform.rotation;

    Tank::Physics::TankInput input;
    input.throttle = 1.0f;
    test.SetInput(input);

    for (int i = 0; i < 300; ++i)
    {
        test.Step(dt);
    }

    const Tank::Physics::TrackedVehicleTestState& state = test.State();
    const float forwardDistance = state.bodyPosition.z - startPosition.z;

    bool passed = true;
    passed &= Check(std::isfinite(forwardDistance), "forward distance must be finite");
    passed &= Check(forwardDistance > 1.0f, "tank must move at least 1 meter forward");
    passed &= Check(std::abs(state.bodyPosition.x - startPosition.x) < 0.5f,
        "straight input must not produce excessive sideways drift");
    passed &= Check(std::isfinite(state.linearVelocity.z), "forward velocity must be finite");
    passed &= Check(state.speedMetersPerSecond > 10.0f,
        "tank must reach 10 m/s under full forward input");
    passed &= Check(state.maximumSpeedMetersPerSecond >= state.speedMetersPerSecond,
        "maximum speed must include current speed");
    passed &= Check(state.zeroToTenTimeSeconds > 0.0f,
        "0-10 m/s time must be measured");
    passed &= Check(state.engineRpm > 0.0f, "engine RPM must be reported");
    passed &= Check(state.transmissionGear > 0, "forward gear must be reported");
    passed &= Check(state.clutchFriction >= 0.0f && state.clutchFriction <= 1.0f,
        "clutch friction must be normalized");
    const Tank::Physics::Quat& finalWheelRotation =
        state.wheels[0].transform.rotation;
    const float wheelRotationDelta =
        std::abs(finalWheelRotation.x - startWheelRotation.x) +
        std::abs(finalWheelRotation.y - startWheelRotation.y) +
        std::abs(finalWheelRotation.z - startWheelRotation.z) +
        std::abs(finalWheelRotation.w - startWheelRotation.w);
    passed &= Check(
        wheelRotationDelta > 0.05f,
        "wheel transform rotation must change while driving");

    if (!passed)
    {
        std::cerr << "  startZ=" << startPosition.z
            << " finalZ=" << state.bodyPosition.z
            << " distance=" << forwardDistance
            << " finalX=" << state.bodyPosition.x << "\n";
        return 1;
    }

    std::cout << "PASS TrackedVehicle forward distance=" << forwardDistance << "\n";
    return 0;
}
