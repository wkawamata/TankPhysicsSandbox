#include "Physics/RollingSafetyStateMapper.h"
#include <iostream>

int main()
{
    const auto state = Tank::Physics::MapRollingSafetyState(
        Tank::Physics::RollingObstructionState::Blocked, true);
    if (!state.obstructionSuspected || !state.recoveryActive)
    {
        std::cerr << "FAIL RollingSafetyStateMapper\n";
        return 1;
    }
    std::cout << "PASS RollingSafetyStateMapper\n";
    return 0;
}
