#include "Physics/MortarAimController.h"

#include <cmath>
#include <iostream>

using namespace Tank::Physics;

int main()
{
    MortarAimController controller;
    bool passed = true;
    passed &= controller.Snapshot().angleDegrees == 0.0f;
    passed &= !controller.Snapshot().canFire;
    const auto first = controller.Update(2.0f);
    passed &= first.canFire;
    passed &= first.rangeMeters > 8.0f && first.rangeMeters < 40.0f;
    const auto maximum = controller.Update(10.0f);
    passed &= maximum.atMaximum && maximum.angleDegrees == 65.0f;
    passed &= std::fabs(maximum.rangeMeters - 40.0f) < 0.001f;
    controller.Reset();
    passed &= controller.Snapshot().angleDegrees == 0.0f;

    if (!passed)
    {
        std::cerr << "FAIL MortarAimController\n";
        return 1;
    }
    std::cout << "PASS MortarAimController\n";
    return 0;
}
