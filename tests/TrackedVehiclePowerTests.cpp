#include "Physics/TrackedVehicleTest.h"

#include <cmath>
#include <iostream>

namespace
{
    bool Check(bool condition, const char* message)
    {
        if (!condition)
        {
            std::cerr << "FAIL TrackedVehicle power: " << message << "\n";
        }
        return condition;
    }
}

int main()
{
    constexpr float dt = 1.0f / 60.0f;

    Tank::Physics::TankSettings settings;
    settings.engineMaxTorqueNm = 900.0f;

    Tank::Physics::TrackedVehicleTest test;
    test.Initialize(settings);
    for (int i = 0; i < 180; ++i)
    {
        test.Step(dt);
    }

    const Tank::Physics::Vec3 startPosition = test.State().bodyPosition;
    Tank::Physics::TankInput input;
    input.throttle = 1.0f;
    test.SetInput(input);
    for (int i = 0; i < 300; ++i)
    {
        test.Step(dt);
    }

    const Tank::Physics::TrackedVehicleTestState& state = test.State();
    const float forwardDistance = state.bodyPosition.z - startPosition.z;
    const float lateralDistance = std::abs(state.bodyPosition.x - startPosition.x);

    bool passed = true;
    passed &= Check(forwardDistance > 50.0f,
        "900 Nm tune must improve forward mobility");
    passed &= Check(lateralDistance < 0.5f,
        "900 Nm tune must remain directionally stable");
    passed &= Check(state.zeroToTenTimeSeconds > 0.0f,
        "acceleration time must be measured");

    if (!passed)
    {
        std::cerr << "  distance=" << forwardDistance
            << " lateral=" << lateralDistance
            << " zeroToTen=" << state.zeroToTenTimeSeconds << "\n";
        return 1;
    }

    std::cout << "PASS TrackedVehicle power distance=" << forwardDistance
        << " lateral=" << lateralDistance
        << " zero_to_ten=" << state.zeroToTenTimeSeconds << "\n";
    return 0;
}
