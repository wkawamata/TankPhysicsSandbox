#pragma once

#include "RollingObstructionDetector.h"

namespace Tank::Physics
{
    struct RollingSafetyState
    {
        bool obstructionSuspected = false;
        bool recoveryActive = false;
    };

    inline RollingSafetyState MapRollingSafetyState(
        RollingObstructionState obstruction,
        bool recoveryActive)
    {
        return { obstruction != RollingObstructionState::Clear, recoveryActive };
    }
}
