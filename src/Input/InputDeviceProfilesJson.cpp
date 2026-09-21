#include "InputDeviceProfilesJson.h"

namespace Tank::Input
{
    namespace
    {
        InputDeviceProfile LoadProfile(const nlohmann::json& json)
        {
            InputDeviceProfile profile;
            profile.id = json.value("id", profile.id);
            profile.name = json.value("name", profile.name);
            profile.vendorId = json.value("vendorId", profile.vendorId);
            profile.productId = json.value("productId", profile.productId);
            profile.leftTrackAxis = json.value("leftTrackAxis", profile.leftTrackAxis);
            profile.rightTrackAxis = json.value("rightTrackAxis", profile.rightTrackAxis);
            profile.leftRollAxis = json.value("leftRollAxis", profile.leftRollAxis);
            profile.rightRollAxis = json.value("rightRollAxis", profile.rightRollAxis);
            profile.invertLeftTrack = json.value("invertLeftTrack", profile.invertLeftTrack);
            profile.invertRightTrack = json.value("invertRightTrack", profile.invertRightTrack);
            profile.invertLeftRoll = json.value("invertLeftRoll", profile.invertLeftRoll);
            profile.invertRightRoll = json.value("invertRightRoll", profile.invertRightRoll);
            profile.neutral = json.value("neutral", profile.neutral);
            profile.neutralTolerance = json.value("neutralTolerance", profile.neutralTolerance);
            profile.deadzone = json.value("deadzone", profile.deadzone);
            profile.brakeButton = json.value("brakeButton", profile.brakeButton);
            profile.fireButton = json.value("fireButton", profile.fireButton);
            return profile;
        }

        nlohmann::json SaveProfile(const InputDeviceProfile& profile)
        {
            return {
                {"id", profile.id},
                {"name", profile.name},
                {"vendorId", profile.vendorId},
                {"productId", profile.productId},
                {"leftTrackAxis", profile.leftTrackAxis},
                {"rightTrackAxis", profile.rightTrackAxis},
                {"leftRollAxis", profile.leftRollAxis},
                {"rightRollAxis", profile.rightRollAxis},
                {"invertLeftTrack", profile.invertLeftTrack},
                {"invertRightTrack", profile.invertRightTrack},
                {"invertLeftRoll", profile.invertLeftRoll},
                {"invertRightRoll", profile.invertRightRoll},
                {"neutral", profile.neutral},
                {"neutralTolerance", profile.neutralTolerance},
                {"deadzone", profile.deadzone},
                {"brakeButton", profile.brakeButton},
                {"fireButton", profile.fireButton},
            };
        }
    }

    InputDeviceProfiles LoadInputDeviceProfiles(const nlohmann::json& json)
    {
        InputDeviceProfiles result;
        result.version = json.value("version", InputDeviceProfiles::CurrentVersion);
        if (json.contains("profiles") && json["profiles"].is_array())
        {
            for (const nlohmann::json& profile : json["profiles"])
            {
                result.profiles.push_back(LoadProfile(profile));
            }
        }
        return result;
    }

    nlohmann::json SaveInputDeviceProfiles(const InputDeviceProfiles& profiles)
    {
        nlohmann::json jsonProfiles = nlohmann::json::array();
        for (const InputDeviceProfile& profile : profiles.profiles)
        {
            jsonProfiles.push_back(SaveProfile(profile));
        }
        return {{"version", InputDeviceProfiles::CurrentVersion}, {"profiles", jsonProfiles}};
    }
}
