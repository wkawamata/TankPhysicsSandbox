#include "Physics/TrackedVehicleTest.h"

#include <cmath>
#include <iostream>

namespace
{
    float MeasureForwardDistance(float floorFriction)
    {
        constexpr float dt = 1.0f / 60.0f;
        Tank::Physics::PhysicsEnvironmentSettings environment;
        environment.floorFriction = floorFriction;
        environment.obstacleCount = 0;

        Tank::Physics::TrackedVehicleTest test;
        test.Initialize({}, environment);
        for (int step = 0; step < 180; ++step)
        {
            test.Step(dt);
        }
        const float startZ = test.State().bodyPosition.z;
        Tank::Physics::TankInput input;
        input.throttle = 1.0f;
        test.SetInput(input);
        for (int step = 0; step < 180; ++step)
        {
            test.Step(dt);
        }
        return test.State().bodyPosition.z - startZ;
    }
}

int main()
{
    const float lowFrictionDistance = MeasureForwardDistance(0.1f);
    const float highFrictionDistance = MeasureForwardDistance(1.0f);
    if (!std::isfinite(lowFrictionDistance) ||
        !std::isfinite(highFrictionDistance) ||
        highFrictionDistance <= lowFrictionDistance + 0.5f)
    {
        std::cerr << "FAIL terrain friction response low=" << lowFrictionDistance
                  << " high=" << highFrictionDistance << '\n';
        return 1;
    }
    std::cout << "PASS terrain friction response low=" << lowFrictionDistance
              << " high=" << highFrictionDistance << '\n';
    return 0;
}
