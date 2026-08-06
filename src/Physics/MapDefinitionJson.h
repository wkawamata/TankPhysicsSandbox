#pragma once

#include "MapDefinition.h"

#include <string>
#include <vector>

namespace Tank::Physics
{
    struct MapDocument
    {
        std::string name;
        PhysicsEnvironmentSettings environment;
        std::vector<MapPrimitive> primitives;
    };

    std::string SerializeMapDocument(const MapDocument& document);
    bool DeserializeMapDocument(
        const std::string& jsonText,
        MapDocument& document,
        std::string* error = nullptr);
}
