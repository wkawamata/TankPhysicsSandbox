#include "Physics/TrackedVehicleTest.h"

#include <cmath>
#include <iostream>
#include <vector>

namespace
{
    float MeasureForwardDistance(
        float floorFriction,
        const std::vector<Tank::Physics::MapPrimitive>& primitives = {},
        const Tank::Physics::MapSpawn& spawn = {})
    {
        constexpr float dt = 1.0f / 60.0f;
        Tank::Physics::PhysicsEnvironmentSettings environment;
        environment.floorFriction = floorFriction;
        environment.obstacleCount = 0;

        Tank::Physics::TrackedVehicleTest test;
        test.Initialize({}, environment, primitives, spawn);
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

    Tank::Physics::MapPrimitive platform;
    platform.type = Tank::Physics::MapPrimitiveType::Box;
    platform.position = { 0.0f, 0.25f, 0.0f };
    platform.size = { 100.0f, 0.5f, 100.0f };
    Tank::Physics::MapSpawn platformSpawn;
    platformSpawn.position = { 0.0f, 2.5f, 0.0f };
    platform.friction = 0.1f;
    const float lowPrimitiveDistance =
        MeasureForwardDistance(0.6f, { platform }, platformSpawn);
    platform.friction = 1.0f;
    const float highPrimitiveDistance =
        MeasureForwardDistance(0.6f, { platform }, platformSpawn);
    if (!std::isfinite(lowPrimitiveDistance) ||
        !std::isfinite(highPrimitiveDistance) ||
        highPrimitiveDistance <= lowPrimitiveDistance + 0.5f)
    {
        std::cerr << "FAIL primitive friction response low=" << lowPrimitiveDistance
                  << " high=" << highPrimitiveDistance << '\n';
        return 1;
    }

    Tank::Physics::MapPrimitive heightField;
    heightField.type = Tank::Physics::MapPrimitiveType::HeightField;
    heightField.position = { 0.0f, 0.05f, 0.0f };
    heightField.heightFieldSampleCount = 16;
    heightField.heightFieldCellSizeM = 5.0f;
    heightField.heightFieldHeights.assign(16 * 16, 0.0f);
    heightField.friction = 0.1f;
    const float lowHeightFieldDistance =
        MeasureForwardDistance(0.6f, { heightField }, platformSpawn);
    heightField.friction = 1.0f;
    const float highHeightFieldDistance =
        MeasureForwardDistance(0.6f, { heightField }, platformSpawn);
    if (!std::isfinite(lowHeightFieldDistance) ||
        !std::isfinite(highHeightFieldDistance) ||
        highHeightFieldDistance <= lowHeightFieldDistance + 0.5f)
    {
        std::cerr << "FAIL height field friction response low="
                  << lowHeightFieldDistance << " high=" << highHeightFieldDistance << '\n';
        return 1;
    }
    std::cout << "PASS terrain friction response low=" << lowFrictionDistance
              << " high=" << highFrictionDistance
              << " primitive_low=" << lowPrimitiveDistance
              << " primitive_high=" << highPrimitiveDistance
              << " height_field_low=" << lowHeightFieldDistance
              << " height_field_high=" << highHeightFieldDistance << '\n';
    return 0;
}
