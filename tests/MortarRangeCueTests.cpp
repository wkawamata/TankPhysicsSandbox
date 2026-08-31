#include "Rendering/MortarRangeCue.h"

#include <iostream>

int main()
{
    Tank::Rendering::MortarRangeCue cue;
    cue.radiusMeters = 20.0f;
    cue.visible = true;
    const bool passed = cue.visible && cue.radiusMeters == 20.0f;
    if (!passed)
    {
        std::cerr << "FAIL MortarRangeCue\n";
        return 1;
    }
    std::cout << "PASS MortarRangeCue\n";
    return 0;
}
