#include "Input/TankInputMappingJson.h"

#include <iostream>

using namespace Tank::Input;

int main()
{
    const auto settings = LoadTankInputMappingSettings({{"leftLeverAxis", 6}});
    const auto saved = SaveTankInputMappingSettings(settings);
    const bool passed = settings.leftLeverAxis == 6 &&
        settings.rightLeverAxis == 3 && saved.value("leftLeverAxis", 0) == 6;
    if (!passed)
    {
        std::cerr << "FAIL TankInputMappingJson\n";
        return 1;
    }
    std::cout << "PASS TankInputMappingJson\n";
    return 0;
}
