#include "MapDefinition.h"
#include "TestObstacleLayout.h"

#include <array>

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

    std::vector<MapPrimitive> BuildMapPrimitives(
        MapId id,
        const PhysicsEnvironmentSettings& environment)
    {
        std::vector<MapPrimitive> primitives;
        if (id == MapId::FlatGround)
        {
            return primitives;
        }

        constexpr std::array<float, 3> frictionBands = { 0.3f, 0.6f, 1.0f };
        const std::vector<TestObstaclePlacement> obstacles =
            GenerateTestObstacleLayout(environment);
        primitives.reserve(obstacles.size());
        for (size_t index = 0; index < obstacles.size(); ++index)
        {
            const TestObstaclePlacement& obstacle = obstacles[index];
            MapPrimitive primitive;
            primitive.type = MapPrimitiveType::Box;
            primitive.position = obstacle.position;
            primitive.size = {
                kPassengerCarWidthM,
                kPassengerCarHeightM,
                kPassengerCarLengthM };
            primitive.yawRadians = obstacle.yawRadians;
            primitive.friction = frictionBands[index % frictionBands.size()];
            primitives.push_back(primitive);
        }
        return primitives;
    }
}
