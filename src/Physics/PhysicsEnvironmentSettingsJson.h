#pragma once

#include "PhysicsEnvironmentSettings.h"

#include <string>

namespace Tank::Physics
{
    std::string SerializePhysicsEnvironmentSettings(
        const PhysicsEnvironmentSettings& settings);
    bool DeserializePhysicsEnvironmentSettings(
        const std::string& jsonText,
        PhysicsEnvironmentSettings& settings,
        std::string* error = nullptr);
}
