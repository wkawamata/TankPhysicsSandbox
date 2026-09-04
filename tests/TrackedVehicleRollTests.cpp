#include "Physics/TrackedVehicleTest.h"

#include <cmath>
#include <iostream>

namespace
{
    float BodyUpY(const Tank::Physics::Quat& rotation)
    {
        return 1.0f - 2.0f *
            (rotation.x * rotation.x + rotation.z * rotation.z);
    }

    bool Check(bool condition, const char* message)
    {
        if (!condition)
        {
            std::cerr << "FAIL TrackedVehicle roll: " << message << "\n";
        }
        return condition;
    }
}

int main()
{
    constexpr float dt = 1.0f / 60.0f;

    Tank::Physics::TrackedVehicleTest test;
    Tank::Physics::TankSettings settings;
    settings.rollingInputEnabled = true;
    Tank::Physics::TrackedVehicleTest earlyTest;
    earlyTest.Initialize(settings);
    Tank::Physics::TankInput earlyInput;
    earlyInput.roll = 1.0f;
    earlyTest.SetInput(earlyInput);
    for (int i = 0; i < 10; ++i)
    {
        earlyTest.Step(dt);
    }
    const float earlyUpY = BodyUpY(earlyTest.State().bodyRotation);
    bool passed = true;
    passed &= Check(earlyUpY > 0.9f,
        "roll must be rejected before mobility reaches Stopped");
    earlyInput.roll = 0.0f;
    earlyTest.SetInput(earlyInput);
    test.Initialize(settings);
    for (int i = 0; i < 180; ++i)
    {
        test.Step(dt);
    }

    const Tank::Physics::Vec3 startPosition = test.State().bodyPosition;
    Tank::Physics::TankInput input;
    input.roll = 1.0f;
    input.leftLeverX = 1.0f;
    input.rightLeverX = 1.0f;
    test.SetInput(input);
    test.Step(dt);
    input.roll = 0.0f;
    input.leftLeverX = 0.0f;
    input.rightLeverX = 0.0f;
    test.SetInput(input);
    float maximumRollSpeed = 0.0f;
    bool sawPoweredRoll = false;
    bool sawEvaluating = false;
    bool sawBallisticRoll = false;
    bool sawSettling = false;
    int airborneFrames = 0;
    int maximumAirborneFrames = 0;
    for (int i = 0; i < 240; ++i)
    {
        test.Step(dt);
        sawPoweredRoll |= test.State().rollingPhase ==
            Tank::Physics::RollingPhase::PoweredRoll;
        sawEvaluating |= test.State().rollingPhase ==
            Tank::Physics::RollingPhase::Evaluating;
        sawBallisticRoll |= test.State().rollingPhase ==
            Tank::Physics::RollingPhase::BallisticRoll;
        sawSettling |= test.State().rollingPhase ==
            Tank::Physics::RollingPhase::Settling;
        airborneFrames = test.State().motionObservation.totalContactCount == 0
            ? airborneFrames + 1 : 0;
        maximumAirborneFrames = (std::max)(
            maximumAirborneFrames,
            airborneFrames);
        maximumRollSpeed = (std::max)(
            maximumRollSpeed,
            std::abs(test.State().angularVelocity.z));
    }

    const float operatedUpY = BodyUpY(test.State().bodyRotation);
    const float heldRollSpeed = std::abs(test.State().angularVelocity.z);

    for (int i = 0; i < 300; ++i)
    {
        test.Step(dt);
        sawSettling |= test.State().rollingPhase ==
            Tank::Physics::RollingPhase::Settling;
    }

    const Tank::Physics::TrackedVehicleTestState& state = test.State();
    const float settledUpY = BodyUpY(state.bodyRotation);
    const float displacementX = state.bodyPosition.x - startPosition.x;
    const float displacementZ = state.bodyPosition.z - startPosition.z;
    const float lateralDistance =
        std::sqrt(displacementX * displacementX + displacementZ * displacementZ);

    passed &= Check(std::isfinite(operatedUpY) && std::isfinite(settledUpY),
        "orientation must remain finite");
    passed &= Check(operatedUpY < 0.5f,
        "roll input must rotate the body away from upright");
    passed &= Check(sawPoweredRoll,
        "rolling must expose PoweredRoll phase");
    passed &= Check(state.specialMove.state !=
            Tank::Physics::SpecialMoveState::RollStarting,
        "physical roll start must leave RollStarting");
    passed &= Check(sawEvaluating,
        "rolling must expose Evaluating phase near the cutoff");
    passed &= Check(sawBallisticRoll,
        "rolling must expose BallisticRoll phase");
    passed &= Check(heldRollSpeed < maximumRollSpeed,
        "held roll input must not keep increasing roll speed after 90 degrees");
    passed &= Check(std::abs(settledUpY) > 0.8f,
        "released roll input must stabilize near upright or inverted");
    passed &= Check(state.mobility.state ==
            Tank::Physics::MobilityState::Stopped,
        "completed roll must return mobility to Stopped");
    passed &= Check(state.specialMove.state ==
            Tank::Physics::SpecialMoveState::Idle,
        "completed roll must return special move to Idle");
    passed &= Check(lateralDistance > 2.5f && lateralDistance < 2.85f,
        "one roll must translate approximately one vehicle width");
    passed &= Check(displacementX > 2.5f,
        "positive same-direction lever input must roll toward vehicle right");
    passed &= Check(maximumAirborneFrames <= 30,
        "air braking must prevent prolonged airborne rotation");

    if (!passed)
    {
        std::cerr << "  operatedUpY=" << operatedUpY
            << " settledUpY=" << settledUpY
            << " maxRollSpeed=" << maximumRollSpeed
            << " heldRollSpeed=" << heldRollSpeed
            << " lateralDistance=" << lateralDistance
            << " mobility=" << static_cast<int>(state.mobility.state)
            << " reason=" << static_cast<int>(state.mobility.lastTransitionReason)
            << " phase=" << static_cast<int>(state.rollingPhase)
            << " special=" << static_cast<int>(state.specialMove.state)
            << " speed=" << state.motionObservation.linearSpeedMetersPerSecond
            << " angular=" << state.motionObservation.angularSpeedRadiansPerSecond
            << " contacts=" << state.motionObservation.totalContactCount
            << " lower=" << state.motionObservation.totalLowerSurfaceContactCount
            << " upper=" << state.motionObservation.totalUpperSurfaceContactCount
            << " slipL=" << state.motionObservation.tracks[0]
                .maximumAbsoluteLongitudinalSlipMetersPerSecond
            << " slipR=" << state.motionObservation.tracks[1]
                .maximumAbsoluteLongitudinalSlipMetersPerSecond
            << " suspensionL=" << state.motionObservation.tracks[0]
                .maximumAbsoluteSuspensionVelocityMetersPerSecond
            << " suspensionR=" << state.motionObservation.tracks[1]
                .maximumAbsoluteSuspensionVelocityMetersPerSecond
            << " maxAirborneFrames=" << maximumAirborneFrames
            << "\n";
        return 1;
    }

    std::cout << "PASS TrackedVehicle roll operated_up_y=" << operatedUpY
        << " settled_up_y=" << settledUpY
        << " lateral_distance=" << lateralDistance
        << " max_airborne_frames=" << maximumAirborneFrames << "\n";
    return 0;
}
