#include "MapDefinition.h"

namespace Tank::Physics
{
    namespace
    {
        PhysicsEnvironmentSettings MakeFlatGround()
        {
            PhysicsEnvironmentSettings settings;
            settings.obstacleCount = 0;
            return settings;
        }

        PhysicsEnvironmentSettings MakeObstacleField()
        {
            PhysicsEnvironmentSettings settings;
            settings.obstacleCount = 20;
            settings.obstacleSeed = 1;
            settings.obstacleAreaSizeM = 100.0f;
            return settings;
        }
    }

    const std::array<MapDefinition, 2>& GetMapDefinitions()
    {
        static const std::array<MapDefinition, 2> definitions = {
            MapDefinition { MapId::FlatGround, "Flat Ground", MakeFlatGround() },
            MapDefinition { MapId::ObstacleField, "Obstacle Field", MakeObstacleField() },
        };
        return definitions;
    }

    const MapDefinition& GetMapDefinition(MapId id)
    {
        for (const MapDefinition& definition : GetMapDefinitions())
        {
            if (definition.id == id)
            {
                return definition;
            }
        }
        return GetMapDefinitions().front();
    }
}
