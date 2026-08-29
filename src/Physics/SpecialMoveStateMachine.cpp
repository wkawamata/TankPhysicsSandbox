#include "SpecialMoveStateMachine.h"

namespace Tank::Physics
{
    const SpecialMoveStateSnapshot& SpecialMoveStateMachine::Update(
        SpecialMoveEvent event,
        bool mobilityStopped)
    {
        m_snapshot.lastEvent = event;
        m_snapshot.lastRejectReason = SpecialMoveRejectReason::None;

        if (event == SpecialMoveEvent::None)
        {
            return m_snapshot;
        }

        if (event == SpecialMoveEvent::MoveBlocked)
        {
            if (IsActive())
            {
                m_snapshot.state = SpecialMoveState::Blocked;
                ++m_snapshot.transitionCount;
            }
            else
            {
                Reject(SpecialMoveRejectReason::Blocked);
            }
            return m_snapshot;
        }

        if (event == SpecialMoveEvent::MoveCompleted)
        {
            if (m_snapshot.state == SpecialMoveState::MortarStarting)
            {
                m_snapshot.state = SpecialMoveState::MortarAiming;
                ++m_snapshot.transitionCount;
            }
            else if (m_snapshot.state == SpecialMoveState::RollStarting)
            {
                m_snapshot.state = SpecialMoveState::Rolling;
                ++m_snapshot.transitionCount;
            }
            else if (m_snapshot.state == SpecialMoveState::Rolling ||
                m_snapshot.state == SpecialMoveState::MortarAiming)
            {
                m_snapshot.state = SpecialMoveState::Idle;
                ++m_snapshot.transitionCount;
            }
            else
            {
                Reject(SpecialMoveRejectReason::InvalidInput);
            }
            return m_snapshot;
        }

        if (event == SpecialMoveEvent::RecoveryStarted)
        {
            if (m_snapshot.state == SpecialMoveState::Blocked)
            {
                m_snapshot.state = SpecialMoveState::RecoveringToStart;
                ++m_snapshot.transitionCount;
            }
            else
            {
                Reject(SpecialMoveRejectReason::InvalidInput);
            }
            return m_snapshot;
        }

        if (event == SpecialMoveEvent::RecoveryCompleted)
        {
            if (m_snapshot.state == SpecialMoveState::RecoveringToStart)
            {
                m_snapshot.state = SpecialMoveState::Idle;
                ++m_snapshot.transitionCount;
            }
            else
            {
                Reject(SpecialMoveRejectReason::InvalidInput);
            }
            return m_snapshot;
        }

        if (m_snapshot.state != SpecialMoveState::Idle)
        {
            Reject(SpecialMoveRejectReason::AlreadyActive);
            return m_snapshot;
        }
        if (!mobilityStopped)
        {
            Reject(SpecialMoveRejectReason::MobilityNotStopped);
            return m_snapshot;
        }

        if (event == SpecialMoveEvent::MortarRequested)
        {
            m_snapshot.state = SpecialMoveState::MortarStarting;
            ++m_snapshot.transitionCount;
        }
        else if (event == SpecialMoveEvent::RollLeftRequested ||
            event == SpecialMoveEvent::RollRightRequested)
        {
            m_snapshot.state = SpecialMoveState::RollStarting;
            ++m_snapshot.transitionCount;
        }
        else
        {
            Reject(SpecialMoveRejectReason::InvalidInput);
        }
        return m_snapshot;
    }

    void SpecialMoveStateMachine::Reset()
    {
        m_snapshot = {};
    }

    bool SpecialMoveStateMachine::IsActive() const
    {
        return m_snapshot.state != SpecialMoveState::Idle;
    }

    void SpecialMoveStateMachine::Reject(SpecialMoveRejectReason reason)
    {
        m_snapshot.lastRejectReason = reason;
    }
}
