#include "Input/TankInputMappingJson.h"
#include "Input/InputDeviceProfilesJson.h"

#include <iostream>

using namespace Tank::Input;

int main()
{
    const auto settings = LoadTankInputMappingSettings({{"leftLeverAxis", 6}});
    const auto saved = SaveTankInputMappingSettings(settings);
    const auto profiles = LoadInputDeviceProfiles({
        {"version", 1},
        {"profiles", {{{"id", "test-pad"}, {"vendorId", 0x056E},
            {"productId", 0x200A}, {"leftTrackAxis", 5}, {"invertLeftTrack", false},
            {"cameraNextButton", 4}, {"cameraPreviousButton", 5}}}}});
    const auto savedProfiles = SaveInputDeviceProfiles(profiles);
    const bool passed = settings.leftLeverAxis == 6 &&
        settings.rightLeverAxis == 3 && saved.value("leftLeverAxis", 0) == 6;
    const bool profilesPassed = profiles.version == 1 && profiles.profiles.size() == 1 &&
        profiles.profiles[0].id == "test-pad" &&
        profiles.profiles[0].vendorId == 0x056E &&
        profiles.profiles[0].productId == 0x200A &&
        profiles.profiles[0].leftTrackAxis == 5 &&
        !profiles.profiles[0].invertLeftTrack &&
        profiles.profiles[0].cameraNextButton == 4 &&
        profiles.profiles[0].cameraPreviousButton == 5 &&
        savedProfiles["profiles"][0].value("rightTrackAxis", 0) == 1;
    if (!passed || !profilesPassed)
    {
        std::cerr << "FAIL TankInputMappingJson\n";
        return 1;
    }
    std::cout << "PASS TankInputMappingJson\n";
    return 0;
}
