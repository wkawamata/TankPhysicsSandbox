#pragma once
#include "NavigationGrid.h"

namespace Tank::Physics
{
    struct EnemyNavigationDriver
    {
        std::string groupId;
        std::string enemyId;
        std::string surfaceId;
        double snapDistanceMeters = 0;
        NavigationMobility mobility;
    };
    struct EnemyNavigationSetup
    {
        NavigationGrid grid;
        std::vector<EnemyNavigationDriver> drivers;
    };
    bool DeserializeEnemyNavigationSetup(const std::string& text, EnemyNavigationSetup& setup, std::string& error);
}
