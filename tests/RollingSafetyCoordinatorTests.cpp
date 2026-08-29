#include "Physics/RollingSafetyCoordinator.h"

#include <iostream>

using namespace Tank::Physics;

int main()
{
    RollingSafetyCoordinator coordinator;
    bool passed = true;
    const RollingSafetyInput blocked{
        RollingPhase::PoweredRoll, RollingObstructionState::Blocked, false};
    passed &= coordinator.Update(blocked) == SpecialMoveEvent::MoveBlocked;
    passed &= coordinator.Update(blocked) == SpecialMoveEvent::None;

    const RollingSafetyInput ready{
        RollingPhase::None, RollingObstructionState::Clear, false};
    passed &= coordinator.Update(ready) == SpecialMoveEvent::RecoveryStarted;
    passed &= coordinator.Update({
        RollingPhase::None, RollingObstructionState::Clear, true}) ==
        SpecialMoveEvent::RecoveryCompleted;
    passed &= coordinator.Update(blocked) == SpecialMoveEvent::MoveBlocked;

    if (!passed)
    {
        std::cerr << "FAIL RollingSafetyCoordinator\n";
        return 1;
    }
    std::cout << "PASS RollingSafetyCoordinator\n";
    return 0;
}
