#pragma once

#include "SpecialActionRecognizer.h"

#include <cstdint>

namespace Tank::Physics
{
    enum class RollingPhase
    {
        None,
        Windup,
        PoweredRoll,
        Evaluating,
        BallisticRoll,
        Settling
    };

    enum class SpecialMoveState
    {
        Idle,
        RollStarting,
        Rolling,
        MortarStarting,
        MortarAiming,
        Blocked
    };

    enum class SpecialMoveEvent
    {
        None,
        RollLeftRequested,
        RollRightRequested,
        MortarRequested,
        MoveBlocked,
        MoveCompleted
    };

    enum class SpecialMoveRejectReason
    {
        None,
        MobilityNotStopped,
        AlreadyActive,
        InvalidInput,
        Blocked
    };

    struct SpecialMoveStateSnapshot
    {
        SpecialMoveState state = SpecialMoveState::Idle;
        SpecialMoveEvent lastEvent = SpecialMoveEvent::None;
        SpecialMoveRejectReason lastRejectReason =
            SpecialMoveRejectReason::None;
        std::uint64_t transitionCount = 0;
    };
}
