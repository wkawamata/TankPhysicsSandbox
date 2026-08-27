#include "Physics/TrackedVehicleTest.h"

#include <cmath>
#include <iostream>

namespace
{
    bool Check(bool condition, const char* message)
    {
        if (!condition)
        {
            std::cerr << "FAIL TrackedVehicle coasting: " << message << "\n";
        }
        return condition;
    }
}

int main()
{
    constexpr float deltaTimeSeconds = 1.0f / 60.0f;

    Tank::Physics::TrackedVehicleTest test;
    test.Initialize();
    for (int step = 0; step < 180; ++step)
    {
        test.Step(deltaTimeSeconds);
    }

    Tank::Physics::TankInput driveInput;
    driveInput.throttle = 1.0f;
    test.SetInput(driveInput);
    for (int step = 0; step < 180; ++step)
    {
        test.Step(deltaTimeSeconds);
    }

    const float drivenSpeed = test.State().speedMetersPerSecond;
    Tank::Physics::TankInput releasedInput;
    test.SetInput(releasedInput);
    const Tank::Physics::TrackedVehicleTestState coastState =
        test.Step(deltaTimeSeconds);
    const float coastSpeed = coastState.speedMetersPerSecond;

    bool passed = true;
    passed &= Check(std::isfinite(drivenSpeed) && std::isfinite(coastSpeed),
        "driven and coasting speed must be finite");
    passed &= Check(drivenSpeed > 5.0f,
        "the vehicle must reach a visible speed before input release");
    passed &= Check(coastSpeed > drivenSpeed * 0.5f,
        "input release must not report an immediate physical stop");
    passed &= Check(coastState.motionObservation.allFinite,
        "coasting motion observation must remain finite");
    passed &= Check(std::abs(
            coastState.motionObservation.horizontalSpeedMetersPerSecond -
            coastState.speedMetersPerSecond) < 0.0001f,
        "observation horizontal speed must match the legacy speed value");
    passed &= Check(
        coastState.motionObservation.localLinearVelocity.z > 0.0f,
        "coasting observation must retain forward local velocity");
    passed &= Check(coastState.motionObservation.hasRequiredDriveContact,
        "coasting vehicle must retain lower contact on both tracks");

    Tank::Physics::TankInput brakeInput;
    brakeInput.brake = true;
    test.SetInput(brakeInput);
    for (int step = 0; step < 180; ++step)
    {
        test.Step(deltaTimeSeconds);
    }
    const float brakedSpeed = test.State().speedMetersPerSecond;
    passed &= Check(std::isfinite(brakedSpeed),
        "braked speed must remain finite");
    passed &= Check(brakedSpeed < coastSpeed,
        "braking must reduce speed from the coasting baseline");

    if (!passed)
    {
        std::cerr << "  driven_speed=" << drivenSpeed
            << " coast_speed=" << coastSpeed
            << " braked_speed=" << brakedSpeed << "\n";
        return 1;
    }

    std::cout << "PASS TrackedVehicle coasting driven_speed=" << drivenSpeed
        << " coast_speed=" << coastSpeed
        << " braked_speed=" << brakedSpeed << "\n";
    return 0;
}
