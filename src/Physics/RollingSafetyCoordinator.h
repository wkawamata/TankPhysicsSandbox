#pragma once

#include "RollingObstructionDetector.h"
#include "SpecialMoveTypes.h"

namespace Tank::Physics
{
    struct RollingSafetyInput
    {
        RollingPhase phase = RollingPhase::None;
        RollingObstructionState obstruction = RollingObstructionState::Clear;
        bool recoveryComplete = false;
    };

    class RollingSafetyCoordinator
    {
    public:
        SpecialMoveEvent Update(const RollingSafetyInput& input);
        void Reset();

    private:
        bool m_blockedLatched = false;
        bool m_recoveryStarted = false;
    };
}
