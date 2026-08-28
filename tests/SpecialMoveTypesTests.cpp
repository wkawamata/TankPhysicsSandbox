#include "Physics/SpecialMoveTypes.h"

#include <iostream>

int main()
{
    using namespace Tank::Physics;
    SpecialMoveStateSnapshot snapshot;
    bool passed = snapshot.state == SpecialMoveState::Idle &&
        snapshot.lastEvent == SpecialMoveEvent::None &&
        snapshot.lastRejectReason == SpecialMoveRejectReason::None &&
        snapshot.transitionCount == 0;
    if (!passed)
    {
        std::cerr << "FAIL SpecialMoveTypes defaults\n";
        return 1;
    }
    std::cout << "PASS SpecialMoveTypes\n";
    return 0;
}
