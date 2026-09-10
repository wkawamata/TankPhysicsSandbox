#include "MortarAimIntegration.h"

namespace Tank::Physics
{
    MortarAimIntegrationResult MortarAimIntegration::Update(
        SpecialMoveState state,
        float deltaTimeSeconds,
        bool fireRequested)
    {
        MortarAimIntegrationResult result;
        if (state != SpecialMoveState::MortarAiming)
        {
            result.aim = m_aimController.Snapshot();
            return result;
        }

        result.aim = m_aimController.Update(deltaTimeSeconds);
        if (fireRequested && result.aim.canFire)
        {
            result.event = SpecialMoveEvent::MoveCompleted;
        }
        return result;
    }

    void MortarAimIntegration::Reset()
    {
        m_aimController.Reset();
    }
}
