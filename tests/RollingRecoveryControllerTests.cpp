#include "Physics/RollingRecoveryController.h"

#include <cmath>
#include <iostream>
#include <limits>

int main()
{
    using namespace Tank::Physics;
    RollingRecoveryController controller;
    bool passed = true;
    passed &= controller.ComputeTorque({ 1.0f, 0.0f, true }) == 30000.0f;
    passed &= controller.ComputeTorque({ 10.0f, 0.0f, true }) == 45000.0f;
    passed &= controller.ComputeTorque({ 1.0f, 0.0f, false }) == 0.0f;
    passed &= std::abs(controller.ComputeTorque({
        1.0f, 10.0f, true }) + 45000.0f) < 0.001f;
    passed &= controller.ComputeTorque({
        (std::numeric_limits<float>::quiet_NaN)(), 0.0f, true }) == 0.0f;
    if (!passed)
    {
        std::cerr << "FAIL RollingRecoveryController\n";
        return 1;
    }
    std::cout << "PASS RollingRecoveryController\n";
    return 0;
}
