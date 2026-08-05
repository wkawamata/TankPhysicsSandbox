#include "Physics/MapDefinition.h"

#include <iostream>

int main()
{
    using namespace Tank::Physics;
    const auto& maps = GetMapDefinitions();
    const MapDefinition& flat = GetMapDefinition(MapId::FlatGround);
    const MapDefinition& obstacles = GetMapDefinition(MapId::ObstacleField);
    const MapDefinition& ramps = GetMapDefinition(MapId::RampCourse);
    const std::vector<MapPrimitive> flatPrimitives =
        BuildMapPrimitives(MapId::FlatGround, flat.environment);
    const std::vector<MapPrimitive> obstaclePrimitives =
        BuildMapPrimitives(MapId::ObstacleField, obstacles.environment);
    const std::vector<MapPrimitive> rampPrimitives =
        BuildMapPrimitives(MapId::RampCourse, ramps.environment);
    if (maps.size() != 3 ||
        flat.environment.obstacleCount != 0 ||
        obstacles.environment.obstacleCount <= 0 ||
        flat.environment.floorSizeM <= 0.0f ||
        obstacles.environment.floorSizeM <= 0.0f ||
        !flatPrimitives.empty() ||
        obstaclePrimitives.empty() ||
        rampPrimitives.size() != 6)
    {
        std::cerr << "FAIL MapDefinition\n";
        return 1;
    }

    bool sawLowFriction = false;
    bool sawHighFriction = false;
    for (const MapPrimitive& primitive : obstaclePrimitives)
    {
        if (primitive.type != MapPrimitiveType::Box ||
            primitive.size.x <= 0.0f ||
            primitive.size.y <= 0.0f ||
            primitive.size.z <= 0.0f)
        {
            return 1;
        }
        sawLowFriction |= primitive.friction < 0.45f;
        sawHighFriction |= primitive.friction >= 0.8f;
    }
    if (!sawLowFriction || !sawHighFriction)
    {
        return 1;
    }
    size_t triangularPrismCount = 0;
    for (const MapPrimitive& primitive : rampPrimitives)
    {
        triangularPrismCount +=
            primitive.type == MapPrimitiveType::TriangularPrism ? 1 : 0;
    }
    if (triangularPrismCount != 3)
    {
        return 1;
    }
    std::cout << "PASS MapDefinition\n";
    return 0;
}
