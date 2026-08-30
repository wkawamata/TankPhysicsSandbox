#include "Physics/SpecialMoveInputProcessor.h"

#include <iostream>

using namespace Tank::Physics;

int main()
{
    SpecialMoveInputProcessor processor;
    SpecialMoveStateMachine machine;
    TankInput input;
    input.leftLeverX = -1.0f;
    input.rightLeverX = 1.0f;
    const auto& snapshot = processor.Update(machine, input, true);
    const bool passed = snapshot.state == SpecialMoveState::MortarStarting;
    if (!passed)
    {
        std::cerr << "FAIL SpecialMoveInputProcessor\n";
        return 1;
    }
    std::cout << "PASS SpecialMoveInputProcessor\n";
    return 0;
}
