#pragma once

#include "PhysicsEnvironmentSettings.h"
#include "PhysicsTypes.h"

#include <array>
#include <vector>

namespace Tank::Physics
{
    enum class MapId
    {
        FlatGround,
        ObstacleField,
        RampCourse,
    };

    enum class MapPrimitiveType
    {
        Box,
        TriangularPrism,
    };

    struct MapPrimitive
    {
        MapPrimitiveType type = MapPrimitiveType::Box;
        Vec3 position = {};
        Vec3 size = { 1.0f, 1.0f, 1.0f };
        float yawRadians = 0.0f;
        float friction = 0.6f;
    };

    struct MapDefinition
    {
        MapId id = MapId::FlatGround;
        const char* name = "Flat Ground";
        PhysicsEnvironmentSettings environment = {};
    };

    const std::array<MapDefinition, 3>& GetMapDefinitions();
    const MapDefinition& GetMapDefinition(MapId id);
    std::vector<MapPrimitive> BuildMapPrimitives(
        MapId id,
        const PhysicsEnvironmentSettings& environment);
}
