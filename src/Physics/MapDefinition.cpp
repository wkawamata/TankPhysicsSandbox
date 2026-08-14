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

    const std::array<MapDefinition, 3>& GetMapDefinitions()
    {
        static const std::array<MapDefinition, 3> definitions = {
            MapDefinition { MapId::FlatGround, "Flat Ground", MakeFlatGround() },
            MapDefinition { MapId::ObstacleField, "Obstacle Field", MakeObstacleField() },
            MapDefinition { MapId::RampCourse, "Ramp Course", MakeFlatGround() },
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

        if (id == MapId::RampCourse)
        {
            constexpr std::array<float, 3> frictionBands = { 0.3f, 0.6f, 1.0f };
            for (size_t index = 0; index < frictionBands.size(); ++index)
            {
                MapPrimitive ramp;
                ramp.type = MapPrimitiveType::TriangularPrism;
                ramp.position = { (static_cast<float>(index) - 1.0f) * 6.0f, 1.5f, 15.0f };
                ramp.size = { 6.0f, 3.0f, 12.0f };
                ramp.friction = frictionBands[index];
                primitives.push_back(ramp);

                MapPrimitive platform;
                platform.type = MapPrimitiveType::Box;
                platform.position = { ramp.position.x, 1.5f, 27.0f };
                platform.size = { 6.0f, 3.0f, 12.0f };
                platform.friction = frictionBands[index];
                primitives.push_back(platform);
            }
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
