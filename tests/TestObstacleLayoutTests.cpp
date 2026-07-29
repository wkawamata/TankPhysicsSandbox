#include "Physics/TestObstacleLayout.h"

#include <cmath>
#include <iostream>

namespace
{
    bool SamePlacement(
        const Tank::Physics::TestObstaclePlacement& left,
        const Tank::Physics::TestObstaclePlacement& right)
    {
        constexpr float epsilon = 0.0001f;
        return std::abs(left.position.x - right.position.x) < epsilon &&
            std::abs(left.position.y - right.position.y) < epsilon &&
            std::abs(left.position.z - right.position.z) < epsilon &&
            std::abs(left.yawRadians - right.yawRadians) < epsilon;
    }
}

int main()
{
    Tank::Physics::PhysicsEnvironmentSettings settings;
    settings.obstacleCount = 20;
    settings.obstacleSeed = 42;
    settings.obstacleAreaSizeM = 100.0f;

    const auto first = Tank::Physics::GenerateTestObstacleLayout(settings);
    const auto second = Tank::Physics::GenerateTestObstacleLayout(settings);
    if (first.size() != 20 || second.size() != first.size())
    {
        std::cerr << "FAIL obstacle layout count\n";
        return 1;
    }

    for (size_t index = 0; index < first.size(); ++index)
    {
        if (!SamePlacement(first[index], second[index]))
        {
            std::cerr << "FAIL obstacle layout is not deterministic\n";
            return 1;
        }

        const float distanceSquared =
            first[index].position.x * first[index].position.x +
            first[index].position.z * first[index].position.z;
        if (distanceSquared < 100.0f ||
            std::abs(first[index].position.x) > 50.0f ||
            std::abs(first[index].position.z) > 50.0f)
        {
            std::cerr << "FAIL obstacle layout bounds\n";
            return 1;
        }
    }

    settings.obstacleSeed = 43;
    const auto differentSeed = Tank::Physics::GenerateTestObstacleLayout(settings);
    if (differentSeed.empty() || SamePlacement(first.front(), differentSeed.front()))
    {
        std::cerr << "FAIL obstacle seed has no effect\n";
        return 1;
    }

    std::cout << "PASS test obstacle layout\n";
    return 0;
}
