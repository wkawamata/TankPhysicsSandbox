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
    const auto& snapshot = processor.Update(machine, input, true, true);
    bool passed = snapshot.state == SpecialMoveState::MortarStarting;

    SpecialMoveInputProcessor chainedProcessor;
    SpecialMoveStateMachine chainedMachine;
    TankInput rollInput;
    rollInput.leftLeverX = 1.0f;
    rollInput.rightLeverX = 1.0f;
    const auto& chainedRoll = chainedProcessor.Update(
        chainedMachine,
        rollInput,
        false,
        true);
    passed = passed && chainedRoll.state == SpecialMoveState::RollStarting;

    SpecialMoveInputProcessor mortarProcessor;
    SpecialMoveStateMachine mortarMachine;
    const auto& rejectedMortar = mortarProcessor.Update(
        mortarMachine,
        input,
        false,
        true);
    passed = passed && rejectedMortar.state == SpecialMoveState::Idle &&
        rejectedMortar.lastRejectReason ==
            SpecialMoveRejectReason::MobilityNotStopped;
    if (!passed)
    {
        std::cerr << "FAIL SpecialMoveInputProcessor\n";
        return 1;
    }
    std::cout << "PASS SpecialMoveInputProcessor\n";
    return 0;
}
