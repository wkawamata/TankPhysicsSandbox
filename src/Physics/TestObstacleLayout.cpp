#include "TestObstacleLayout.h"

#include <algorithm>
#include <cmath>
#include <random>

namespace Tank::Physics
{
    std::vector<TestObstaclePlacement> GenerateTestObstacleLayout(
        const PhysicsEnvironmentSettings& settings)
    {
        constexpr float spawnClearRadiusM = 10.0f;
        constexpr float minimumSeparationM = 6.0f;
        constexpr int maximumAttemptsPerObstacle = 64;

        const int obstacleCount = std::clamp(settings.obstacleCount, 0, 100);
        const float halfArea =
            0.5f * std::clamp(settings.obstacleAreaSizeM, 20.0f, 500.0f);
        std::mt19937 random(static_cast<std::mt19937::result_type>(settings.obstacleSeed));
        std::uniform_real_distribution<float> positionDistribution(-halfArea, halfArea);
        std::uniform_real_distribution<float> yawDistribution(
            0.0f,
            2.0f * 3.14159265358979323846f);

        std::vector<TestObstaclePlacement> placements;
        placements.reserve(static_cast<size_t>(obstacleCount));
        for (int obstacle = 0; obstacle < obstacleCount; ++obstacle)
        {
            for (int attempt = 0; attempt < maximumAttemptsPerObstacle; ++attempt)
            {
                TestObstaclePlacement placement;
                placement.position = {
                    positionDistribution(random),
                    0.5f * kPassengerCarHeightM,
                    positionDistribution(random)};
                placement.yawRadians = yawDistribution(random);

                const float spawnDistanceSquared =
                    placement.position.x * placement.position.x +
                    placement.position.z * placement.position.z;
                if (spawnDistanceSquared < spawnClearRadiusM * spawnClearRadiusM)
                {
                    continue;
                }

                bool overlaps = false;
                for (const TestObstaclePlacement& existing : placements)
                {
                    const float dx = placement.position.x - existing.position.x;
                    const float dz = placement.position.z - existing.position.z;
                    if (dx * dx + dz * dz <
                        minimumSeparationM * minimumSeparationM)
                    {
                        overlaps = true;
                        break;
                    }
                }
                if (!overlaps)
                {
                    placements.push_back(placement);
                    break;
                }
            }
        }
        return placements;
    }
}
