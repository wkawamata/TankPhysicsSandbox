#pragma once

#include "MortarAimController.h"
#include "SpecialMoveTypes.h"

namespace Tank::Physics
{
    struct MortarAimIntegrationResult
    {
        MortarAimSnapshot aim = {};
        SpecialMoveEvent event = SpecialMoveEvent::None;
    };

    class MortarAimIntegration
    {
    public:
        MortarAimIntegrationResult Update(
            SpecialMoveState state,
            float deltaTimeSeconds,
            bool fireRequested);
        void Reset();

    private:
        MortarAimController m_aimController;
    };
}
