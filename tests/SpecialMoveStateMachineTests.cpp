#include "Physics/SpecialMoveStateMachine.h"

#include <iostream>

namespace
{
    using namespace Tank::Physics;

    bool Check(bool condition, const char* message)
    {
        if (!condition)
        {
            std::cerr << "FAIL SpecialMoveStateMachine: " << message << "\n";
        }
        return condition;
    }
}

int main()
{
    using namespace Tank::Physics;
    bool passed = true;
    SpecialMoveStateMachine machine;

    const SpecialMoveStateSnapshot& movingReject = machine.Update(
        SpecialMoveEvent::RollLeftRequested, false);
    passed &= Check(movingReject.state == SpecialMoveState::Idle,
        "moving mobility must not start a special move");
    passed &= Check(movingReject.lastRejectReason ==
        SpecialMoveRejectReason::MobilityNotStopped,
        "moving rejection must report MobilityNotStopped");

    const SpecialMoveStateSnapshot& rollStart = machine.Update(
        SpecialMoveEvent::RollLeftRequested, true);
    passed &= Check(rollStart.state == SpecialMoveState::RollStarting,
        "stopped mobility must accept roll request");
    passed &= Check(rollStart.transitionCount == 1,
        "accepted request must count one transition");
    passed &= Check(rollStart.requestedRollSign < 0.0f,
        "left roll must preserve a negative request sign");

    const SpecialMoveStateSnapshot& activeReject = machine.Update(
        SpecialMoveEvent::MortarRequested, true);
    passed &= Check(activeReject.state == SpecialMoveState::RollStarting,
        "mortar must not start while roll is active");
    passed &= Check(activeReject.lastRejectReason ==
        SpecialMoveRejectReason::AlreadyActive,
        "active conflict must report AlreadyActive");

    machine.Update(SpecialMoveEvent::MoveCompleted, true);
    passed &= Check(machine.Snapshot().state == SpecialMoveState::Rolling,
        "roll starting completion must enter Rolling");
    passed &= Check(machine.Snapshot().requestedRollSign < 0.0f,
        "MoveCompleted must not erase the pending roll direction");
    machine.Update(SpecialMoveEvent::MoveBlocked, true);
    passed &= Check(machine.Snapshot().state == SpecialMoveState::Blocked,
        "active move must enter Blocked on obstruction");
    machine.Update(SpecialMoveEvent::RecoveryStarted, true);
    passed &= Check(machine.Snapshot().state ==
            SpecialMoveState::RecoveringToStart,
        "blocked move must enter recovery before returning idle");
    machine.Update(SpecialMoveEvent::RecoveryCompleted, true);
    passed &= Check(machine.Snapshot().state == SpecialMoveState::Idle,
        "blocked move completion must return to Idle");

    machine.Update(SpecialMoveEvent::MortarRequested, true);
    passed &= Check(machine.Snapshot().state == SpecialMoveState::MortarStarting,
        "stopped mobility must accept mortar request");
    machine.Update(SpecialMoveEvent::MoveCompleted, true);
    passed &= Check(machine.Snapshot().state == SpecialMoveState::MortarAiming,
        "mortar starting completion must enter MortarAiming");
    machine.Update(SpecialMoveEvent::MoveCompleted, true);
    passed &= Check(machine.Snapshot().state == SpecialMoveState::Idle,
        "mortar completion must return to Idle");

    machine.Update(SpecialMoveEvent::MoveBlocked, true);
    passed &= Check(machine.Snapshot().lastRejectReason ==
        SpecialMoveRejectReason::Blocked,
        "blocked event while idle must be rejected");
    machine.Reset();
    passed &= Check(machine.Snapshot().transitionCount == 0 &&
        machine.Snapshot().state == SpecialMoveState::Idle &&
        machine.Snapshot().requestedRollSign == 0.0f,
        "reset must restore initial state");

    if (!passed)
    {
        return 1;
    }
    std::cout << "PASS SpecialMoveStateMachine\n";
    return 0;
}
