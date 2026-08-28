#pragma once

#include "TankTypes.h"

namespace Tank::Physics
{
    enum class MobilityState
    {
        Moving,
        StopCandidate,
        Stopped
    };

    enum class MobilityTransitionReason
    {
        None,
        StopConditionsEntered,
        StopConfirmed,
        DriveRequested,
        LinearSpeedExceeded,
        AngularSpeedExceeded,
        TrackSlipExceeded,
        SuspensionUnstable,
        RequiredContactLost,
        PoseUnstable,
        InvalidObservation
    };

    struct MobilityStateSettings
    {
        float stoppedEnterLinearSpeedMetersPerSecond = 0.20f;
        float stoppedExitLinearSpeedMetersPerSecond = 0.35f;
        float stoppedEnterAngularSpeedRadiansPerSecond = 0.20f;
        float stoppedExitAngularSpeedRadiansPerSecond = 0.35f;
        float stoppedEnterTrackSlipMetersPerSecond = 0.20f;
        float stoppedExitTrackSlipMetersPerSecond = 0.40f;
        float stoppedEnterSuspensionSpeedMetersPerSecond = 0.10f;
        float stoppedExitSuspensionSpeedMetersPerSecond = 0.20f;
        float stoppedMinimumUpAlignment = 0.90f;
        float stoppedConfirmSeconds = 0.25f;
    };

    struct MobilityStateSnapshot
    {
        MobilityState state = MobilityState::Moving;
        float stateTimeSeconds = 0.0f;
        float stopCandidateProgress = 0.0f;
        MobilityTransitionReason lastTransitionReason =
            MobilityTransitionReason::None;
    };

    class MobilityStateMachine
    {
    public:
        explicit MobilityStateMachine(
            const MobilityStateSettings& settings = {});

        void Reset();
        const MobilityStateSnapshot& Update(
            const TankMotionObservation& observation,
            bool driveRequested,
            float deltaTimeSeconds);
        const MobilityStateSnapshot& Snapshot() const;

    private:
        MobilityTransitionReason EvaluateEntryFailure(
            const TankMotionObservation& observation,
            bool driveRequested) const;
        MobilityTransitionReason EvaluateExitFailure(
            const TankMotionObservation& observation,
            bool driveRequested) const;
        void TransitionTo(
            MobilityState state,
            MobilityTransitionReason reason);
        void UpdateProgress();

        MobilityStateSettings m_settings = {};
        MobilityStateSnapshot m_snapshot = {};
    };
}
