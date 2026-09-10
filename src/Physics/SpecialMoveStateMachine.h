#pragma once

#include "SpecialMoveTypes.h"

namespace Tank::Physics
{
    class SpecialMoveStateMachine
    {
    public:
        const SpecialMoveStateSnapshot& Update(
            SpecialMoveEvent event,
            bool mobilityStopped);
        const SpecialMoveStateSnapshot& Snapshot() const
        {
            return m_snapshot;
        }
        void Reset();

    private:
        bool IsActive() const;
        void Reject(SpecialMoveRejectReason reason);

        SpecialMoveStateSnapshot m_snapshot = {};
    };
}
