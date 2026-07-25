#pragma once

#include "TankTypes.h"

#include <string>

namespace Tank::Physics
{
    std::string SerializeTankSettings(const TankSettings& settings);
    bool DeserializeTankSettings(
        const std::string& jsonText,
        TankSettings& settings,
        std::string* error = nullptr);
}
