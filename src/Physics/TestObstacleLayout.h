#pragma once

#include "PhysicsEnvironmentSettings.h"
#include "PhysicsTypes.h"

#include <vector>

namespace Tank::Physics
{
    struct TestObstaclePlacement
    {
        Vec3 position = {};
        float yawRadians = 0.0f;
    };

    constexpr float kPassengerCarWidthM = 2.7f;
    constexpr float kPassengerCarHeightM = 2.25f;
    constexpr float kPassengerCarLengthM = 6.75f;

    std::vector<TestObstaclePlacement> GenerateTestObstacleLayout(
        const PhysicsEnvironmentSettings& settings);
}
