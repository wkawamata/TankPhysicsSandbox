#include "Physics/TrackedVehicleTest.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <vector>

int main()
{
    constexpr float dt = 1.0f / 60.0f;
    Tank::Physics::PhysicsEnvironmentSettings environment;
    environment.obstacleCount = 0;

    Tank::Physics::MapPrimitive ramp;
    ramp.type = Tank::Physics::MapPrimitiveType::TriangularPrism;
    ramp.position = { 0.0f, 1.0f, 12.0f };
    ramp.size = { 5.0f, 2.0f, 8.0f };
    ramp.friction = 1.0f;

    Tank::Physics::MapPrimitive platform;
    platform.type = Tank::Physics::MapPrimitiveType::Box;
    platform.position = { 0.0f, 1.0f, 20.0f };
    platform.size = { 5.0f, 2.0f, 8.0f };
    platform.friction = 1.0f;

    Tank::Physics::TrackedVehicleTest test;
    test.Initialize({}, environment, { ramp, platform });
    for (int step = 0; step < 180; ++step)
    {
        test.Step(dt);
    }
    const float startZ = test.State().bodyPosition.z;
    float maximumY = test.State().bodyPosition.y;
    Tank::Physics::TankInput input;
    input.throttle = 1.0f;
    test.SetInput(input);
    for (int step = 0; step < 360; ++step)
    {
        const Tank::Physics::TrackedVehicleTestState state = test.Step(dt);
        maximumY = (std::max)(maximumY, state.bodyPosition.y);
    }

    const Tank::Physics::TrackedVehicleTestState& state = test.State();
    const float forwardDistance = state.bodyPosition.z - startZ;
    if (!std::isfinite(maximumY) || !std::isfinite(forwardDistance) ||
        maximumY < 2.0f || forwardDistance < 20.0f || state.bodyPosition.y < 0.0f)
    {
        std::cerr << "FAIL ramp traversal max_y=" << maximumY
                  << " distance=" << forwardDistance
                  << " final_y=" << state.bodyPosition.y << '\n';
        return 1;
    }

    std::cout << "PASS ramp traversal max_y=" << maximumY
              << " distance=" << forwardDistance
              << " final_y=" << state.bodyPosition.y << '\n';
    return 0;
}
