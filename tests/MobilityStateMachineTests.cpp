#include "Physics/MobilityStateMachine.h"

#include <cmath>
#include <iostream>
#include <limits>

namespace
{
    using Tank::Physics::MobilityState;
    using Tank::Physics::MobilityStateMachine;
    using Tank::Physics::MobilityStateSnapshot;
    using Tank::Physics::MobilityTransitionReason;
    using Tank::Physics::TankMotionObservation;

    bool Check(bool condition, const char* message)
    {
        if (!condition)
        {
            std::cerr << "FAIL MobilityStateMachine: " << message << "\n";
        }
        return condition;
    }

    TankMotionObservation MakeStoppedObservation()
    {
        TankMotionObservation observation;
        observation.bodyUp = { 0.0f, 1.0f, 0.0f };
        observation.averageContactNormal = { 0.0f, 1.0f, 0.0f };
        observation.hasRequiredDriveContact = true;
        observation.allFinite = true;
        return observation;
    }

    bool ReachStopped(MobilityStateMachine& machine)
    {
        const TankMotionObservation observation = MakeStoppedObservation();
        machine.Update(observation, false, 0.10f);
        machine.Update(observation, false, 0.10f);
        machine.Update(observation, false, 0.10f);
        return machine.Update(observation, false, 0.05f).state ==
            MobilityState::Stopped;
    }

    bool CheckCandidateFailure(
        const TankMotionObservation& failedObservation,
        bool driveRequested,
        MobilityTransitionReason expectedReason,
        const char* message)
    {
        MobilityStateMachine machine;
        machine.Update(MakeStoppedObservation(), false, 0.01f);
        const MobilityStateSnapshot& snapshot = machine.Update(
            failedObservation, driveRequested, 0.01f);
        return Check(
            snapshot.state == MobilityState::Moving &&
                snapshot.lastTransitionReason == expectedReason,
            message);
    }

    bool CheckStoppedFailure(
        const TankMotionObservation& failedObservation,
        bool driveRequested,
        MobilityTransitionReason expectedReason,
        const char* message)
    {
        MobilityStateMachine machine;
        if (!ReachStopped(machine))
        {
            return Check(false, "stopped failure setup must reach Stopped");
        }
        const MobilityStateSnapshot& snapshot = machine.Update(
            failedObservation, driveRequested, 0.01f);
        return Check(
            snapshot.state == MobilityState::Moving &&
                snapshot.lastTransitionReason == expectedReason,
            message);
    }
}

int main()
{
    bool passed = true;
    const TankMotionObservation stopped = MakeStoppedObservation();

    MobilityStateMachine normal;
    const MobilityStateSnapshot& candidate = normal.Update(
        stopped, false, 0.10f);
    passed &= Check(candidate.state == MobilityState::StopCandidate,
        "stable observation must enter StopCandidate");
    passed &= Check(candidate.lastTransitionReason ==
        MobilityTransitionReason::StopConditionsEntered,
        "entry reason must be StopConditionsEntered");
    passed &= Check(candidate.stateTimeSeconds == 0.0f,
        "a transition must reset state time");

    const MobilityStateSnapshot& progressing = normal.Update(
        stopped, false, 0.10f);
    passed &= Check(progressing.state == MobilityState::StopCandidate,
        "confirmation time must prevent an early Stopped transition");
    passed &= Check(std::abs(progressing.stopCandidateProgress - 0.4f) <
        0.0001f,
        "candidate progress must expose normalized confirmation time");

    normal.Update(stopped, false, 0.10f);
    const MobilityStateSnapshot& confirmed = normal.Update(
        stopped, false, 0.05f);
    passed &= Check(confirmed.state == MobilityState::Stopped,
        "0.25 seconds of stable candidate time must confirm Stopped");
    passed &= Check(confirmed.lastTransitionReason ==
        MobilityTransitionReason::StopConfirmed,
        "confirmation reason must be StopConfirmed");
    passed &= Check(confirmed.stopCandidateProgress == 0.0f,
        "progress must reset after leaving StopCandidate");

    TankMotionObservation invertedStopped = stopped;
    invertedStopped.bodyUp = { 0.0f, -1.0f, 0.0f };
    MobilityStateMachine invertedMachine;
    invertedMachine.Update(invertedStopped, false, 0.10f);
    invertedMachine.Update(invertedStopped, false, 0.10f);
    invertedMachine.Update(invertedStopped, false, 0.10f);
    passed &= Check(
        invertedMachine.Update(invertedStopped, false, 0.05f).state ==
            MobilityState::Stopped,
        "stable inverted pose must reach Stopped");

    TankMotionObservation hysteresis = stopped;
    hysteresis.linearSpeedMetersPerSecond = 0.25f;
    const MobilityStateSnapshot& heldStopped = normal.Update(
        hysteresis, false, 0.01f);
    passed &= Check(heldStopped.state == MobilityState::Stopped,
        "value between enter and exit thresholds must remain Stopped");

    MobilityStateMachine movingHysteresis;
    const MobilityStateSnapshot& heldMoving = movingHysteresis.Update(
        hysteresis, false, 0.01f);
    passed &= Check(heldMoving.state == MobilityState::Moving,
        "value between enter and exit thresholds must remain Moving");

    TankMotionObservation failed = stopped;
    passed &= CheckCandidateFailure(
        failed, true, MobilityTransitionReason::DriveRequested,
        "drive request must cancel StopCandidate");

    failed = stopped;
    failed.linearSpeedMetersPerSecond = 0.20f;
    passed &= CheckCandidateFailure(
        failed, false, MobilityTransitionReason::LinearSpeedExceeded,
        "linear entry threshold must cancel StopCandidate");

    failed = stopped;
    failed.angularSpeedRadiansPerSecond = 0.20f;
    passed &= CheckCandidateFailure(
        failed, false, MobilityTransitionReason::AngularSpeedExceeded,
        "angular entry threshold must cancel StopCandidate");

    failed = stopped;
    failed.tracks[0].maximumAbsoluteLongitudinalSlipMetersPerSecond = 0.20f;
    passed &= CheckCandidateFailure(
        failed, false, MobilityTransitionReason::TrackSlipExceeded,
        "track-slip entry threshold must cancel StopCandidate");

    failed = stopped;
    failed.tracks[1]
        .maximumAbsoluteSuspensionVelocityMetersPerSecond = 0.10f;
    passed &= CheckCandidateFailure(
        failed, false, MobilityTransitionReason::SuspensionUnstable,
        "suspension entry threshold must cancel StopCandidate");

    failed = stopped;
    failed.hasRequiredDriveContact = false;
    passed &= CheckCandidateFailure(
        failed, false, MobilityTransitionReason::RequiredContactLost,
        "missing drive contact must cancel StopCandidate");

    failed = stopped;
    failed.averageContactNormal = { 1.0f, 0.0f, 0.0f };
    passed &= CheckCandidateFailure(
        failed, false, MobilityTransitionReason::PoseUnstable,
        "unstable pose must cancel StopCandidate");

    failed = stopped;
    failed.linearSpeedMetersPerSecond =
        (std::numeric_limits<float>::quiet_NaN)();
    passed &= CheckCandidateFailure(
        failed, false, MobilityTransitionReason::InvalidObservation,
        "non-finite values must safely cancel StopCandidate");

    TankMotionObservation exitObservation = stopped;
    passed &= CheckStoppedFailure(
        exitObservation, true, MobilityTransitionReason::DriveRequested,
        "drive request must leave Stopped before physical speed rises");

    exitObservation = stopped;
    exitObservation.linearSpeedMetersPerSecond = 0.35f;
    passed &= CheckStoppedFailure(
        exitObservation, false,
        MobilityTransitionReason::LinearSpeedExceeded,
        "linear exit threshold must return Stopped to Moving");

    exitObservation = stopped;
    exitObservation.angularSpeedRadiansPerSecond = 0.35f;
    passed &= CheckStoppedFailure(
        exitObservation, false,
        MobilityTransitionReason::AngularSpeedExceeded,
        "angular exit threshold must return Stopped to Moving");

    exitObservation = stopped;
    exitObservation.tracks[1]
        .maximumAbsoluteLongitudinalSlipMetersPerSecond = 0.40f;
    passed &= CheckStoppedFailure(
        exitObservation, false,
        MobilityTransitionReason::TrackSlipExceeded,
        "exit threshold must return Stopped to Moving with a reason");

    exitObservation = stopped;
    exitObservation.tracks[0]
        .maximumAbsoluteSuspensionVelocityMetersPerSecond = 0.20f;
    passed &= CheckStoppedFailure(
        exitObservation, false,
        MobilityTransitionReason::SuspensionUnstable,
        "suspension exit threshold must return Stopped to Moving");

    exitObservation = stopped;
    exitObservation.hasRequiredDriveContact = false;
    passed &= CheckStoppedFailure(
        exitObservation, false,
        MobilityTransitionReason::RequiredContactLost,
        "contact loss must return Stopped to Moving");

    exitObservation = stopped;
    exitObservation.averageContactNormal = { 1.0f, 0.0f, 0.0f };
    passed &= CheckStoppedFailure(
        exitObservation, false, MobilityTransitionReason::PoseUnstable,
        "unstable pose must return Stopped to Moving");

    exitObservation = stopped;
    exitObservation.allFinite = false;
    passed &= CheckStoppedFailure(
        exitObservation, false,
        MobilityTransitionReason::InvalidObservation,
        "invalid observation must return Stopped to Moving");

    Tank::Physics::MobilityStateSettings immediateSettings;
    immediateSettings.stoppedConfirmSeconds = 0.0f;
    MobilityStateMachine oneTransition(immediateSettings);
    const MobilityStateSnapshot& firstUpdate = oneTransition.Update(
        stopped, false, 1.0f);
    passed &= Check(firstUpdate.state == MobilityState::StopCandidate,
        "one update must perform at most one transition");
    const MobilityStateSnapshot& secondUpdate = oneTransition.Update(
        stopped, false, 0.0f);
    passed &= Check(secondUpdate.state == MobilityState::Stopped,
        "zero confirmation time must confirm on the next update");

    oneTransition.Reset();
    passed &= Check(
        oneTransition.Snapshot().state == MobilityState::Moving &&
            oneTransition.Snapshot().lastTransitionReason ==
                MobilityTransitionReason::None,
        "Reset must restore the initial snapshot");

    if (!passed)
    {
        return 1;
    }

    std::cout << "PASS MobilityStateMachine\n";
    return 0;
}
