#include "Physics/TrackedVehicleTest.h"

#include <cmath>
#include <iostream>

namespace
{
    bool Check(bool condition, const char* message)
    {
        if (!condition)
        {
            std::cerr << "FAIL TrackedVehicle recoil: " << message << "\n";
        }
        return condition;
    }
}

int main()
{
    constexpr float dt = 1.0f / 60.0f;
    constexpr float recoilImpulseNewtonSeconds = 20000.0f;

    Tank::Physics::TrackedVehicleTest test;
    test.Initialize();
    for (int i = 0; i < 180; ++i)
    {
        test.Step(dt);
    }

    const float startZ = test.State().bodyPosition.z;
    bool passed = true;
    passed &= Check(!test.ApplyRecoilImpulse(0.0f), "zero impulse must be rejected");
    passed &= Check(!test.ApplyRecoilImpulse(-1.0f), "negative impulse must be rejected");
    passed &= Check(test.ApplyRecoilImpulse(recoilImpulseNewtonSeconds),
        "positive impulse must be accepted");

    const Tank::Physics::TrackedVehicleTestState state = test.Step(dt);
    passed &= Check(std::isfinite(state.linearVelocity.z), "velocity must be finite");
    passed &= Check(state.linearVelocity.z < -0.1f,
        "forward-facing tank must recoil toward negative Z");
    passed &= Check(state.bodyPosition.z < startZ,
        "recoil must move the tank backward after one step");

    if (!passed)
    {
        std::cerr << "  startZ=" << startZ
                  << " finalZ=" << state.bodyPosition.z
                  << " velocityZ=" << state.linearVelocity.z << "\n";
        return 1;
    }

    std::cout << "PASS TrackedVehicle recoil velocity_z=" << state.linearVelocity.z << "\n";
    return 0;
}
