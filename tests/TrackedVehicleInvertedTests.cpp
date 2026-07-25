#include "Physics/TrackedVehicleTest.h"

#include <cmath>
#include <iostream>

namespace
{
    bool Check(bool condition, const char* message)
    {
        if (!condition)
        {
            std::cerr << "FAIL TrackedVehicle inverted: " << message << "\n";
        }
        return condition;
    }
}

int main()
{
    constexpr float dt = 1.0f / 60.0f;

    Tank::Physics::TankSettings settings;
    settings.startUpsideDown = true;

    Tank::Physics::TrackedVehicleTest test;
    test.Initialize(settings);

    for (int i = 0; i < 180; ++i)
    {
        test.Step(dt);
    }

    const Tank::Physics::Vec3 startPosition = test.State().bodyPosition;
    const Tank::Physics::Quat settledRotation = test.State().bodyRotation;
    int lowerContactCount = 0;
    int upperContactCount = 0;
    for (int i = 0; i < test.State().wheelCount; ++i)
    {
        const Tank::Physics::TrackedWheelState& wheel =
            test.State().wheels[static_cast<size_t>(i)];
        lowerContactCount += !wheel.upperSurface && wheel.hasContact ? 1 : 0;
        upperContactCount += wheel.upperSurface && wheel.hasContact ? 1 : 0;
    }

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
    passed &= Check(upperContactCount > 0,
        "upper surface wheels must contact the floor while inverted");
    passed &= Check(std::isfinite(forwardDistance), "forward distance must be finite");
    passed &= Check(forwardDistance > 1.0f,
        "inverted tank must move at least 1 meter forward");
    passed &= Check(std::abs(state.bodyPosition.x - startPosition.x) < 0.5f,
        "inverted straight input must not produce excessive sideways drift");

    if (!passed)
    {
        std::cerr << "  lowerContacts=" << lowerContactCount
            << " upperContacts=" << upperContactCount
            << " settledRotation=("
            << settledRotation.x << ", " << settledRotation.y << ", "
            << settledRotation.z << ", " << settledRotation.w << ")"
            << " startZ=" << startPosition.z
            << " finalZ=" << state.bodyPosition.z
            << " distance=" << forwardDistance
            << " finalX=" << state.bodyPosition.x << "\n";
        return 1;
    }

    std::cout << "PASS TrackedVehicle inverted upper_contacts=" << upperContactCount
        << " distance=" << forwardDistance << "\n";
    return 0;
}
