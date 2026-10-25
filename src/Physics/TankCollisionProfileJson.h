#pragma once

#include "TankCollisionProfile.h"

#include <nlohmann/json_fwd.hpp>

#include <string>

namespace Tank::Physics
{
    nlohmann::json SerializeTankCollisionProfile(
        const TankCollisionProfile& profile);
    bool DeserializeTankCollisionProfile(
        const nlohmann::json& json,
        TankCollisionProfile& profile,
        std::string& error);
}
