#include "Physics/TrackedVehicleTest.h"

#include <algorithm>
#include <cmath>
#include <iostream>

int main()
{
    constexpr float dt = 1.0f / 60.0f;
    Tank::Physics::PhysicsEnvironmentSettings environment;
    environment.obstacleCount = 0;

    Tank::Physics::MapPrimitive stepBlock;
    stepBlock.type = Tank::Physics::MapPrimitiveType::Box;
    stepBlock.position = { 0.0f, 0.25f, 10.0f };
    stepBlock.size = { 6.0f, 0.5f, 4.0f };
    stepBlock.friction = 1.0f;

    Tank::Physics::TrackedVehicleTest test;
    test.Initialize({}, environment, { stepBlock });
    for (int step = 0; step < 180; ++step)
    {
        test.Step(dt);
    }
    const float startZ = test.State().bodyPosition.z;
    const float settledY = test.State().bodyPosition.y;
    float maximumY = settledY;
    Tank::Physics::TankInput input;
    input.throttle = 1.0f;
    test.SetInput(input);
    for (int step = 0; step < 300; ++step)
    {
        const Tank::Physics::TrackedVehicleTestState state = test.Step(dt);
        maximumY = (std::max)(maximumY, state.bodyPosition.y);
    }

    const Tank::Physics::TrackedVehicleTestState& state = test.State();
    const float forwardDistance = state.bodyPosition.z - startZ;
    const float climbHeight = maximumY - settledY;
    if (!std::isfinite(climbHeight) || !std::isfinite(forwardDistance) ||
        climbHeight < 0.3f || forwardDistance < 20.0f ||
        std::abs(state.bodyPosition.y - settledY) > 0.15f)
    {
        std::cerr << "FAIL step traversal climb=" << climbHeight
                  << " distance=" << forwardDistance
                  << " settled_y=" << settledY
                  << " final_y=" << state.bodyPosition.y << '\n';
        return 1;
    }

    std::cout << "PASS step traversal climb=" << climbHeight
              << " distance=" << forwardDistance
              << " final_y=" << state.bodyPosition.y << '\n';
    return 0;
}
