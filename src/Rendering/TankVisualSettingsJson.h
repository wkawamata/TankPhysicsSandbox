#pragma once

#include "TankVisualSettings.h"

#include <string>

namespace Tank::Rendering
{
    std::string SerializeTankVisualSettings(const TankVisualSettings& settings);
    bool DeserializeTankVisualSettings(
        const std::string& jsonText,
        TankVisualSettings& settings,
        std::string* error = nullptr);
}
