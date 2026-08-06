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
        HeightField,
    };

    struct MapPrimitive
    {
        MapPrimitiveType type = MapPrimitiveType::Box;
        Vec3 position = {};
        Vec3 size = { 1.0f, 1.0f, 1.0f };
        float yawRadians = 0.0f;
        float friction = 0.6f;
        uint32_t heightFieldSampleCount = 0;
        float heightFieldCellSizeM = 1.0f;
        std::vector<float> heightFieldHeights;
    };

    struct MapSpawn
    {
        Vec3 position = { 0.0f, 2.0f, 0.0f };
        float yawRadians = 0.0f;
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
