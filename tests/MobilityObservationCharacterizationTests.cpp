#include "Physics/TrackedVehicleTest.h"

#include <algorithm>
#include <cmath>
#include <iostream>

namespace
{
    struct Metrics
    {
        float linearSpeed = 0.0f;
        float angularSpeed = 0.0f;
        float maximumTrackSlip = 0.0f;
        float maximumSuspensionSpeed = 0.0f;
        float upContactAlignment = 0.0f;
        int lowerContacts = 0;
        bool hasRequiredDriveContact = false;
        bool allFinite = false;
    };

    float Dot(
        const Tank::Physics::Vec3& lhs,
        const Tank::Physics::Vec3& rhs)
    {
        return lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z;
    }

    Metrics Capture(const Tank::Physics::TrackedVehicleTestState& state)
    {
        const Tank::Physics::TankMotionObservation& observation =
            state.motionObservation;
        Metrics metrics;
        metrics.linearSpeed = observation.linearSpeedMetersPerSecond;
        metrics.angularSpeed = observation.angularSpeedRadiansPerSecond;
        metrics.maximumTrackSlip = (std::max)(
            observation.tracks[0]
                .maximumAbsoluteLongitudinalSlipMetersPerSecond,
            observation.tracks[1]
                .maximumAbsoluteLongitudinalSlipMetersPerSecond);
        metrics.maximumSuspensionSpeed = (std::max)(
            observation.tracks[0]
                .maximumAbsoluteSuspensionVelocityMetersPerSecond,
            observation.tracks[1]
                .maximumAbsoluteSuspensionVelocityMetersPerSecond);
        metrics.upContactAlignment = Dot(
            observation.bodyUp,
            observation.averageContactNormal);
        metrics.lowerContacts = observation.totalLowerSurfaceContactCount;
        metrics.hasRequiredDriveContact =
            observation.hasRequiredDriveContact;
        metrics.allFinite = observation.allFinite;
        return metrics;
    }

    bool Check(bool condition, const char* message)
    {
        if (!condition)
        {
            std::cerr << "FAIL Mobility observation: " << message << "\n";
        }
        return condition;
    }

    void PrintMetrics(const char* name, const Metrics& metrics)
    {
        std::cout << name
            << " linear=" << metrics.linearSpeed
            << " angular=" << metrics.angularSpeed
            << " slip=" << metrics.maximumTrackSlip
            << " suspension=" << metrics.maximumSuspensionSpeed
            << " up_contact=" << metrics.upContactAlignment
            << " lower_contacts=" << metrics.lowerContacts
            << " drive_contact=" << (metrics.hasRequiredDriveContact ? 1 : 0)
            << " finite=" << (metrics.allFinite ? 1 : 0)
            << "\n";
    }

    void SettleVehicle(Tank::Physics::TrackedVehicleTest& test)
    {
        constexpr float deltaTimeSeconds = 1.0f / 60.0f;
        test.Initialize();
        for (int step = 0; step < 180; ++step)
        {
            test.Step(deltaTimeSeconds);
        }
    }
}

int main()
{
    constexpr float deltaTimeSeconds = 1.0f / 60.0f;
    bool passed = true;

    Tank::Physics::TrackedVehicleTest stoppedTest;
    SettleVehicle(stoppedTest);
    const Metrics stopped = Capture(stoppedTest.State());
    PrintMetrics("stopped", stopped);
    passed &= Check(stopped.allFinite,
        "stopped observation must be finite");
    passed &= Check(stopped.linearSpeed < 0.2f,
        "stopped linear speed must remain below the candidate threshold");
    passed &= Check(stopped.angularSpeed < 0.2f,
        "stopped angular speed must remain below the candidate threshold");
    passed &= Check(stopped.maximumTrackSlip < 0.2f,
        "stopped track slip must remain below the candidate threshold");
    passed &= Check(stopped.hasRequiredDriveContact,
        "stopped vehicle must retain drive contact on both tracks");

    Tank::Physics::TrackedVehicleTest coastTest;
    SettleVehicle(coastTest);
    Tank::Physics::TankInput driveInput;
    driveInput.throttle = 1.0f;
    coastTest.SetInput(driveInput);
    for (int step = 0; step < 180; ++step)
    {
        coastTest.Step(deltaTimeSeconds);
    }
    Tank::Physics::TankInput releasedInput;
    coastTest.SetInput(releasedInput);
    const Metrics coasting = Capture(coastTest.Step(deltaTimeSeconds));
    PrintMetrics("coasting", coasting);
    passed &= Check(coasting.allFinite,
        "coasting observation must be finite");
    passed &= Check(coasting.linearSpeed > 5.0f,
        "coasting speed must remain clearly above the stopped threshold");

    Tank::Physics::TrackedVehicleTest pivotTest;
    SettleVehicle(pivotTest);
    Tank::Physics::TankInput pivotInput;
    pivotInput.throttle = 1.0f;
    pivotInput.leftTrack = -1.0f;
    pivotInput.rightTrack = 1.0f;
    pivotTest.SetInput(pivotInput);
    for (int step = 0; step < 60; ++step)
    {
        pivotTest.Step(deltaTimeSeconds);
    }
    const Metrics pivot = Capture(pivotTest.State());
    PrintMetrics("pivot", pivot);
    passed &= Check(pivot.allFinite,
        "pivot observation must be finite");
    passed &= Check(pivot.angularSpeed > 0.2f || pivot.maximumTrackSlip > 0.2f,
        "pivot motion must remain distinguishable from stopped");

    Tank::Physics::TrackedVehicleTest stationaryTurnTest;
    SettleVehicle(stationaryTurnTest);
    Tank::Physics::TankInput stationaryTurnInput;
    stationaryTurnInput.throttle = 1.0f;
    stationaryTurnInput.leftTrack = 0.0f;
    stationaryTurnInput.rightTrack = 1.0f;
    stationaryTurnTest.SetInput(stationaryTurnInput);
    for (int step = 0; step < 60; ++step)
    {
        stationaryTurnTest.Step(deltaTimeSeconds);
    }
    const Metrics stationaryTurn = Capture(stationaryTurnTest.State());
    PrintMetrics("stationary_turn", stationaryTurn);
    passed &= Check(stationaryTurn.allFinite,
        "stationary-turn observation must be finite");
    passed &= Check(
        stationaryTurn.linearSpeed > 0.2f ||
            stationaryTurn.angularSpeed > 0.2f ||
            stationaryTurn.maximumTrackSlip > 0.2f,
        "stationary turn must remain distinguishable from stopped");

    Tank::Physics::TankInput brakeInput;
    brakeInput.brake = true;
    coastTest.SetInput(brakeInput);
    int earliestCandidateStep = -1;
    int stableCandidateStartStep = -1;
    int confirmedCandidateStep = -1;
    int consecutiveCandidateSteps = 0;
    Metrics braked;
    for (int step = 1; step <= 300; ++step)
    {
        braked = Capture(coastTest.Step(deltaTimeSeconds));
        const bool candidate =
            braked.allFinite &&
            braked.linearSpeed < 0.2f &&
            braked.angularSpeed < 0.2f &&
            braked.maximumTrackSlip < 0.2f &&
            braked.maximumSuspensionSpeed < 0.1f &&
            braked.hasRequiredDriveContact;
        if (candidate)
        {
            if (earliestCandidateStep < 0)
            {
                earliestCandidateStep = step;
            }
            if (consecutiveCandidateSteps == 0)
            {
                stableCandidateStartStep = step;
            }
            ++consecutiveCandidateSteps;
            if (consecutiveCandidateSteps >= 15)
            {
                confirmedCandidateStep = step;
                break;
            }
        }
        else
        {
            consecutiveCandidateSteps = 0;
            stableCandidateStartStep = -1;
        }
    }
    PrintMetrics("braked_confirmed", braked);
    std::cout << "brake earliest_candidate_seconds="
        << (earliestCandidateStep > 0
            ? earliestCandidateStep * deltaTimeSeconds
            : -1.0f)
        << " stable_candidate_seconds="
        << (stableCandidateStartStep > 0
            ? stableCandidateStartStep * deltaTimeSeconds
            : -1.0f)
        << " confirmed_seconds="
        << (confirmedCandidateStep > 0
            ? confirmedCandidateStep * deltaTimeSeconds
            : -1.0f)
        << "\n";
    passed &= Check(earliestCandidateStep > 0,
        "braking must eventually enter the stop candidate thresholds");
    passed &= Check(confirmedCandidateStep > 0,
        "braking must maintain stop thresholds for 0.25 seconds");

    if (!passed)
    {
        return 1;
    }

    std::cout << "PASS Mobility observation characterization\n";
    return 0;
}
