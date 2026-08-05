#include "Physics/MapDefinition.h"

#include <iostream>

int main()
{
    using namespace Tank::Physics;
    const auto& maps = GetMapDefinitions();
    const MapDefinition& flat = GetMapDefinition(MapId::FlatGround);
    const MapDefinition& obstacles = GetMapDefinition(MapId::ObstacleField);
    if (maps.size() != 2 ||
        flat.environment.obstacleCount != 0 ||
        obstacles.environment.obstacleCount <= 0 ||
        flat.environment.floorSizeM <= 0.0f ||
        obstacles.environment.floorSizeM <= 0.0f)
    {
        std::cerr << "FAIL MapDefinition\n";
        return 1;
    }
    std::cout << "PASS MapDefinition\n";
    return 0;
}
