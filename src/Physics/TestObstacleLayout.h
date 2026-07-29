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

    constexpr float kPassengerCarWidthM = 1.8f;
    constexpr float kPassengerCarHeightM = 1.5f;
    constexpr float kPassengerCarLengthM = 4.5f;

    std::vector<TestObstaclePlacement> GenerateTestObstacleLayout(
        const PhysicsEnvironmentSettings& settings);
}
