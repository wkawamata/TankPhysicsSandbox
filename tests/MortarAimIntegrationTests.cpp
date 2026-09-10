#include "Physics/MortarAimIntegration.h"

#include <iostream>

using namespace Tank::Physics;

int main()
{
    MortarAimIntegration integration;
    bool passed = true;
    auto starting = integration.Update(
        SpecialMoveState::MortarStarting, 1.0f, true);
    passed &= starting.event == SpecialMoveEvent::None;
    auto aiming = integration.Update(
        SpecialMoveState::MortarAiming, 2.0f, false);
    passed &= aiming.aim.canFire;
    passed &= integration.Update(
        SpecialMoveState::MortarAiming, 0.0f, true).event ==
        SpecialMoveEvent::MoveCompleted;
    integration.Reset();
    passed &= integration.Update(
        SpecialMoveState::MortarAiming, 0.0f, true).event ==
        SpecialMoveEvent::None;

    if (!passed)
    {
        std::cerr << "FAIL MortarAimIntegration\n";
        return 1;
    }
    std::cout << "PASS MortarAimIntegration\n";
    return 0;
}
