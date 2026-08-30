#pragma once

#include "TankInputMapper.h"

#include <nlohmann/json.hpp>

namespace Tank::Input
{
    TankInputMappingSettings LoadTankInputMappingSettings(
        const nlohmann::json& json);
    nlohmann::json SaveTankInputMappingSettings(
        const TankInputMappingSettings& settings);
}
