#pragma once

#include "PhysicsEnvironmentSettings.h"

#include <array>

namespace Tank::Physics
{
    enum class MapId
    {
        FlatGround,
        ObstacleField,
    };

    struct MapDefinition
    {
        MapId id = MapId::FlatGround;
        const char* name = "Flat Ground";
        PhysicsEnvironmentSettings environment = {};
    };

    const std::array<MapDefinition, 2>& GetMapDefinitions();
    const MapDefinition& GetMapDefinition(MapId id);
}
