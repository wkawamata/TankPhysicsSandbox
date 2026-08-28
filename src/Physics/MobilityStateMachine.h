#pragma once

#include "TankTypes.h"
#include "MobilityTypes.h"

namespace Tank::Physics
{
    class MobilityStateMachine
    {
    public:
        explicit MobilityStateMachine(
            const MobilityStateSettings& settings = {});

        explicit MobilityStateMachine(const TankSettings& settings)
            : MobilityStateMachine(MobilityStateSettings{
                settings.stoppedEnterLinearSpeedMetersPerSecond,
                settings.stoppedExitLinearSpeedMetersPerSecond,
                settings.stoppedEnterAngularSpeedRadiansPerSecond,
                settings.stoppedExitAngularSpeedRadiansPerSecond,
                settings.stoppedEnterTrackSlipMetersPerSecond,
                settings.stoppedExitTrackSlipMetersPerSecond,
                settings.stoppedEnterSuspensionSpeedMetersPerSecond,
                settings.stoppedExitSuspensionSpeedMetersPerSecond,
                settings.stoppedMinimumUpAlignment,
                settings.stoppedConfirmSeconds })
        {
        }

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
