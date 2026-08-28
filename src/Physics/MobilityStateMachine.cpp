#include "MobilityStateMachine.h"

#include <algorithm>
#include <cmath>

namespace Tank::Physics
{
    namespace
    {
        float Dot(const Vec3& lhs, const Vec3& rhs)
        {
            return lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z;
        }

        float MaximumTrackSlip(const TankMotionObservation& observation)
        {
            return (std::max)(
                observation.tracks[0]
                    .maximumAbsoluteLongitudinalSlipMetersPerSecond,
                observation.tracks[1]
                    .maximumAbsoluteLongitudinalSlipMetersPerSecond);
        }

        float MaximumSuspensionSpeed(
            const TankMotionObservation& observation)
        {
            return (std::max)(
                observation.tracks[0]
                    .maximumAbsoluteSuspensionVelocityMetersPerSecond,
                observation.tracks[1]
                    .maximumAbsoluteSuspensionVelocityMetersPerSecond);
        }

        bool HasFiniteStateValues(const TankMotionObservation& observation)
        {
            return observation.allFinite &&
                std::isfinite(observation.linearSpeedMetersPerSecond) &&
                std::isfinite(observation.angularSpeedRadiansPerSecond) &&
                std::isfinite(MaximumTrackSlip(observation)) &&
                std::isfinite(MaximumSuspensionSpeed(observation)) &&
                std::isfinite(Dot(
                    observation.bodyUp,
                    observation.averageContactNormal));
        }
    }

    MobilityStateMachine::MobilityStateMachine(
        const MobilityStateSettings& settings)
        : m_settings(settings)
    {
    }

    void MobilityStateMachine::Reset()
    {
        m_snapshot = {};
    }

    const MobilityStateSnapshot& MobilityStateMachine::Update(
        const TankMotionObservation& observation,
        bool driveRequested,
        float deltaTimeSeconds)
    {
        const float elapsedSeconds =
            std::isfinite(deltaTimeSeconds) && deltaTimeSeconds > 0.0f
            ? deltaTimeSeconds
            : 0.0f;
        m_snapshot.stateTimeSeconds += elapsedSeconds;

        switch (m_snapshot.state)
        {
        case MobilityState::Moving:
            if (EvaluateEntryFailure(observation, driveRequested) ==
                MobilityTransitionReason::None)
            {
                TransitionTo(
                    MobilityState::StopCandidate,
                    MobilityTransitionReason::StopConditionsEntered);
            }
            break;

        case MobilityState::StopCandidate:
        {
            const MobilityTransitionReason failure =
                EvaluateEntryFailure(observation, driveRequested);
            if (failure != MobilityTransitionReason::None)
            {
                TransitionTo(MobilityState::Moving, failure);
            }
            else if (m_snapshot.stateTimeSeconds >=
                m_settings.stoppedConfirmSeconds)
            {
                TransitionTo(
                    MobilityState::Stopped,
                    MobilityTransitionReason::StopConfirmed);
            }
            break;
        }

        case MobilityState::Stopped:
        {
            const MobilityTransitionReason failure =
                EvaluateExitFailure(observation, driveRequested);
            if (failure != MobilityTransitionReason::None)
            {
                TransitionTo(MobilityState::Moving, failure);
            }
            break;
        }
        }

        UpdateProgress();
        return m_snapshot;
    }

    const MobilityStateSnapshot& MobilityStateMachine::Snapshot() const
    {
        return m_snapshot;
    }

    MobilityTransitionReason MobilityStateMachine::EvaluateEntryFailure(
        const TankMotionObservation& observation,
        bool driveRequested) const
    {
        if (driveRequested)
        {
            return MobilityTransitionReason::DriveRequested;
        }
        if (!HasFiniteStateValues(observation))
        {
            return MobilityTransitionReason::InvalidObservation;
        }
        if (!observation.hasRequiredDriveContact)
        {
            return MobilityTransitionReason::RequiredContactLost;
        }
        if (Dot(observation.bodyUp, observation.averageContactNormal) <
            m_settings.stoppedMinimumUpAlignment)
        {
            return MobilityTransitionReason::PoseUnstable;
        }
        if (observation.linearSpeedMetersPerSecond >=
            m_settings.stoppedEnterLinearSpeedMetersPerSecond)
        {
            return MobilityTransitionReason::LinearSpeedExceeded;
        }
        if (observation.angularSpeedRadiansPerSecond >=
            m_settings.stoppedEnterAngularSpeedRadiansPerSecond)
        {
            return MobilityTransitionReason::AngularSpeedExceeded;
        }
        if (MaximumTrackSlip(observation) >=
            m_settings.stoppedEnterTrackSlipMetersPerSecond)
        {
            return MobilityTransitionReason::TrackSlipExceeded;
        }
        if (MaximumSuspensionSpeed(observation) >=
            m_settings.stoppedEnterSuspensionSpeedMetersPerSecond)
        {
            return MobilityTransitionReason::SuspensionUnstable;
        }
        return MobilityTransitionReason::None;
    }

    MobilityTransitionReason MobilityStateMachine::EvaluateExitFailure(
        const TankMotionObservation& observation,
        bool driveRequested) const
    {
        if (driveRequested)
        {
            return MobilityTransitionReason::DriveRequested;
        }
        if (!HasFiniteStateValues(observation))
        {
            return MobilityTransitionReason::InvalidObservation;
        }
        if (!observation.hasRequiredDriveContact)
        {
            return MobilityTransitionReason::RequiredContactLost;
        }
        if (Dot(observation.bodyUp, observation.averageContactNormal) <
            m_settings.stoppedMinimumUpAlignment)
        {
            return MobilityTransitionReason::PoseUnstable;
        }
        if (observation.linearSpeedMetersPerSecond >=
            m_settings.stoppedExitLinearSpeedMetersPerSecond)
        {
            return MobilityTransitionReason::LinearSpeedExceeded;
        }
        if (observation.angularSpeedRadiansPerSecond >=
            m_settings.stoppedExitAngularSpeedRadiansPerSecond)
        {
            return MobilityTransitionReason::AngularSpeedExceeded;
        }
        if (MaximumTrackSlip(observation) >=
            m_settings.stoppedExitTrackSlipMetersPerSecond)
        {
            return MobilityTransitionReason::TrackSlipExceeded;
        }
        if (MaximumSuspensionSpeed(observation) >=
            m_settings.stoppedExitSuspensionSpeedMetersPerSecond)
        {
            return MobilityTransitionReason::SuspensionUnstable;
        }
        return MobilityTransitionReason::None;
    }

    void MobilityStateMachine::TransitionTo(
        MobilityState state,
        MobilityTransitionReason reason)
    {
        m_snapshot.state = state;
        m_snapshot.stateTimeSeconds = 0.0f;
        m_snapshot.lastTransitionReason = reason;
    }

    void MobilityStateMachine::UpdateProgress()
    {
        if (m_snapshot.state != MobilityState::StopCandidate)
        {
            m_snapshot.stopCandidateProgress = 0.0f;
            return;
        }

        if (m_settings.stoppedConfirmSeconds <= 0.0f)
        {
            m_snapshot.stopCandidateProgress = 1.0f;
            return;
        }

        m_snapshot.stopCandidateProgress = std::clamp(
            m_snapshot.stateTimeSeconds / m_settings.stoppedConfirmSeconds,
            0.0f,
            1.0f);
    }
}
