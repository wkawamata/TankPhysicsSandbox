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
    // Match the default sandbox slot so repeated rolling is verified with
    // the visual tank's wide chassis and track layout, not only defaults.
    settings.chassisWidthM = 2.4f;
    settings.chassisLengthM = 3.92f;
    settings.trackSpacingM = 4.63f;
    settings.trackWidthM = 0.63f;
    settings.roadWheelCount = 4;
    settings.rideHeightScale = 1.1f;
    settings.rollTorqueNm = 200000.0f;
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

    // A follow-up roll must be accepted while the previous roll is still
    // sliding. Landing makes the chain eligible before lateral speed is zero.
    Tank::Physics::TrackedVehicleTest chainTest;
    chainTest.Initialize(settings);
    for (int i = 0; i < 180; ++i)
    {
        chainTest.Step(dt);
    }
    Tank::Physics::TankInput chainInput;
    chainInput.leftLeverX = 1.0f;
    chainInput.rightLeverX = 1.0f;
    chainTest.SetInput(chainInput);
    chainTest.Step(dt);
    chainInput.leftLeverX = 0.0f;
    chainInput.rightLeverX = 0.0f;
    chainTest.SetInput(chainInput);
    bool chainRequestedWhileSliding = false;
    bool chainStarted = false;
    bool chainStartedWhileSliding = false;
    for (int i = 0; i < 480; ++i)
    {
        chainTest.Step(dt);
        const auto& chainState = chainTest.State();
        const bool sliding =
            chainState.motionObservation.linearSpeedMetersPerSecond > 0.20f;
        if (!chainRequestedWhileSliding && chainState.rollChainAvailable &&
            sliding)
        {
            chainInput.leftLeverX = 1.0f;
            chainInput.rightLeverX = 1.0f;
            chainTest.SetInput(chainInput);
            chainRequestedWhileSliding = true;
            continue;
        }
        if (chainRequestedWhileSliding)
        {
            chainInput.leftLeverX = 0.0f;
            chainInput.rightLeverX = 0.0f;
            chainTest.SetInput(chainInput);
            chainStarted |= chainState.rollingPhase ==
                Tank::Physics::RollingPhase::PoweredRoll ||
                chainState.rollingPhase ==
                    Tank::Physics::RollingPhase::Evaluating ||
                chainState.rollingPhase ==
                    Tank::Physics::RollingPhase::BallisticRoll;
            chainStartedWhileSliding |= chainStarted && sliding;
            if (chainStarted)
            {
                break;
            }
        }
    }
    passed &= Check(chainRequestedWhileSliding,
        "roll chain must become available before lateral slide stops");
    passed &= Check(chainStarted,
        "a roll requested during lateral slide must start");
    passed &= Check(chainStartedWhileSliding,
        "a chained roll must start before lateral slide stops");

    Tank::Physics::TrackedVehicleTest returnTest;
    returnTest.Initialize(settings);
    for (int i = 0; i < 180; ++i)
    {
        returnTest.Step(dt);
    }
    const Tank::Physics::Vec3 returnStart = returnTest.State().bodyPosition;
    Tank::Physics::TankInput returnInput;
    returnInput.leftLeverX = 1.0f;
    returnInput.rightLeverX = 1.0f;
    returnTest.SetInput(returnInput);
    returnTest.Step(dt);
    returnInput.leftLeverX = 0.0f;
    returnInput.rightLeverX = 0.0f;
    returnTest.SetInput(returnInput);
    bool requestedReturn = false;
    for (int i = 0; i < 720; ++i)
    {
        if (!requestedReturn && returnTest.State().rollingPhase ==
            Tank::Physics::RollingPhase::Evaluating)
        {
            returnInput.leftLeverX = -1.0f;
            returnInput.rightLeverX = -1.0f;
            returnTest.SetInput(returnInput);
            requestedReturn = true;
        }
        else if (requestedReturn)
        {
            returnInput.leftLeverX = 0.0f;
            returnInput.rightLeverX = 0.0f;
            returnTest.SetInput(returnInput);
        }
        returnTest.Step(dt);
    }
    const Tank::Physics::Vec3 returnEnd = returnTest.State().bodyPosition;
    const float returnDistance = std::sqrt(
        (returnEnd.x - returnStart.x) * (returnEnd.x - returnStart.x) +
        (returnEnd.z - returnStart.z) * (returnEnd.z - returnStart.z));
    passed &= Check(requestedReturn,
        "reverse lever pair must be sampled at the quarter-turn decision");
    passed &= Check(returnDistance < 1.0f,
        "reverse input at 90 degrees must return near the starting position");
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
    float minimumRollDisplacementX = 0.0f;
    float cutoffDisplacementX = 0.0f;
    int framesToInverted = -1;
    int framesToSettling = -1;
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
        if (framesToSettling < 0 && test.State().rollingPhase ==
            Tank::Physics::RollingPhase::Settling)
        {
            framesToSettling = i + 1;
        }
        const float currentDisplacementX =
            test.State().bodyPosition.x - startPosition.x;
        minimumRollDisplacementX = (std::min)(
            minimumRollDisplacementX,
            currentDisplacementX);
        if (test.State().rollingPhase ==
            Tank::Physics::RollingPhase::Evaluating)
        {
            cutoffDisplacementX = currentDisplacementX;
        }
        if (framesToInverted < 0 &&
            BodyUpY(test.State().bodyRotation) < -0.95f)
        {
            framesToInverted = i + 1;
        }
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

    const Tank::Physics::TrackedVehicleTestState state = test.State();
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
    passed &= Check(state.rollingPhase == Tank::Physics::RollingPhase::None,
        "completed roll must clear its physical phase before a second roll");
    passed &= Check(lateralDistance > 5.0f && lateralDistance < 5.6f,
        "one roll must translate approximately one vehicle width");
    passed &= Check(displacementX < -5.0f,
        "positive lever sign must apply positive model-Z roll rotation");
    passed &= Check(minimumRollDisplacementX < -5.0f,
        "the tank must travel in its model-Z rotation fall direction");
    passed &= Check(cutoffDisplacementX < -1.0f,
        "roll translation must begin before the 90 degree cutoff");
    passed &= Check(framesToInverted > 0 && framesToInverted <= 75,
        "the evasive roll must reach its inverted attitude quickly");
    passed &= Check(framesToSettling > 0 && framesToInverted > 0 &&
            framesToSettling - framesToInverted <= 5,
        "landed rolls must promptly leave BallisticRoll for Settling");
    passed &= Check(maximumAirborneFrames <= 65,
        "air braking must prevent prolonged airborne rotation");

    const float firstRollEndX = state.bodyPosition.x;
    input.roll = 1.0f;
    input.leftLeverX = 1.0f;
    input.rightLeverX = 1.0f;
    test.SetInput(input);
    test.Step(dt);
    input.roll = 0.0f;
    input.leftLeverX = 0.0f;
    input.rightLeverX = 0.0f;
    test.SetInput(input);
    for (int i = 0; i < 540; ++i)
    {
        test.Step(dt);
    }
    const float secondRollDisplacementX =
        test.State().bodyPosition.x - firstRollEndX;
    passed &= Check(secondRollDisplacementX > 5.0f,
        "an inverted roll must travel toward its rotating upper surface");
    passed &= Check(test.State().mobility.state ==
            Tank::Physics::MobilityState::Stopped,
        "a second roll must also return mobility to Stopped");

    if (!passed)
    {
        std::cerr << "  operatedUpY=" << operatedUpY
            << " settledUpY=" << settledUpY
            << " maxRollSpeed=" << maximumRollSpeed
            << " heldRollSpeed=" << heldRollSpeed
            << " lateralDistance=" << lateralDistance
            << " minimumRollDisplacementX=" << minimumRollDisplacementX
            << " cutoffDisplacementX=" << cutoffDisplacementX
            << " framesToInverted=" << framesToInverted
            << " framesToSettling=" << framesToSettling
            << " firstPhase=" << static_cast<int>(state.rollingPhase)
            << " firstSpecial=" << static_cast<int>(state.specialMove.state)
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
            << " secondRollDisplacementX=" << secondRollDisplacementX
            << "\n";
        return 1;
    }

    std::cout << "PASS TrackedVehicle roll operated_up_y=" << operatedUpY
        << " settled_up_y=" << settledUpY
        << " lateral_distance=" << lateralDistance
        << " max_airborne_frames=" << maximumAirborneFrames
        << " second_roll_dx=" << secondRollDisplacementX << "\n";
    return 0;
}
