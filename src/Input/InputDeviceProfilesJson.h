#pragma once

#include "InputDeviceProfiles.h"

#include <nlohmann/json.hpp>

namespace Tank::Input
{
    InputDeviceProfiles LoadInputDeviceProfiles(const nlohmann::json& json);
    nlohmann::json SaveInputDeviceProfiles(const InputDeviceProfiles& profiles);
}
