#include "Physics/MortarCameraCue.h"

#include <iostream>

using namespace Tank::Physics;

int main()
{
    const auto cue = MortarCameraCue::FromWheelieProgress(0.5f);
    const auto clamped = MortarCameraCue::FromWheelieProgress(2.0f);
    const bool passed = cue.progress == 0.5f && cue.pitchOffsetDegrees == 9.0f &&
        cue.rangeVisibilityRequired && clamped.progress == 1.0f;
    if (!passed)
    {
        std::cerr << "FAIL MortarCameraCue\n";
        return 1;
    }
    std::cout << "PASS MortarCameraCue\n";
    return 0;
}
