#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace Tank::Input
{
    struct InputDeviceProfile
    {
        std::string id;
        std::string name;
        std::uint16_t vendorId = 0;
        std::uint16_t productId = 0;
        std::size_t leftTrackAxis = 3;
        std::size_t rightTrackAxis = 1;
        std::size_t leftRollAxis = 0;
        std::size_t rightRollAxis = 2;
        bool invertLeftTrack = true;
        bool invertRightTrack = true;
        bool invertLeftRoll = false;
        bool invertRightRoll = false;
        float neutral = 0.5f;
        float neutralTolerance = 0.05f;
        float deadzone = 0.1f;
        std::uint32_t brakeButton = 3;
        std::uint32_t fireButton = 13;
    };

    struct InputDeviceProfiles
    {
        static constexpr int CurrentVersion = 1;

        int version = CurrentVersion;
        std::vector<InputDeviceProfile> profiles;
    };
}
