#include "RollingSafetyCoordinator.h"

namespace Tank::Physics
{
    SpecialMoveEvent RollingSafetyCoordinator::Update(
        const RollingSafetyInput& input)
    {
        if (input.obstruction == RollingObstructionState::Blocked &&
            !m_blockedLatched)
        {
            m_blockedLatched = true;
            return SpecialMoveEvent::MoveBlocked;
        }

        if (m_blockedLatched && !m_recoveryStarted &&
            input.phase == RollingPhase::None)
        {
            m_recoveryStarted = true;
            return SpecialMoveEvent::RecoveryStarted;
        }

        if (m_recoveryStarted && input.recoveryComplete)
        {
            Reset();
            return SpecialMoveEvent::RecoveryCompleted;
        }

        return SpecialMoveEvent::None;
    }

    void RollingSafetyCoordinator::Reset()
    {
        m_blockedLatched = false;
        m_recoveryStarted = false;
    }
}
