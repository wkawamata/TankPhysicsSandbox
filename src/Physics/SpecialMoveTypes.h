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
        CommitRoll,
        BallisticRoll,
        Settling
    };

    enum class RollingDecision
    {
        None,
        ContinueForward,
        ReturnToStart
    };

    enum class RollingTraceEvent
    {
        None,
        StartLatched,
        ContinueForward,
        ReturnToStart,
        Finished
    };

    enum class SpecialMoveState
    {
        Idle,
        RollStarting,
        Rolling,
        MortarStarting,
        MortarAiming,
        Blocked,
        RecoveringToStart
    };

    enum class SpecialMoveEvent
    {
        None,
        RollLeftRequested,
        RollRightRequested,
        MortarRequested,
        MoveBlocked,
        MoveCompleted,
        RecoveryStarted,
        RecoveryCompleted
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
        // Preserved through MoveCompleted so a delayed physical start cannot
        // mistake a left roll for the default right roll.
        float requestedRollSign = 0.0f;
        SpecialMoveRejectReason lastRejectReason =
            SpecialMoveRejectReason::None;
        std::uint64_t transitionCount = 0;
    };
}
