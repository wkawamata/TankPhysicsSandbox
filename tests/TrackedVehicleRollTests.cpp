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

    float BodyUpX(const Tank::Physics::Quat& rotation)
    {
        return 2.0f *
            (rotation.x * rotation.y - rotation.z * rotation.w);
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

    // The public DirectX convention is +Z forward, +X right, +Y up. A
    // negative paired horizontal lever command is a negative local-Z roll,
    // therefore its upper face must initially fall toward local -X.
    Tank::Physics::TrackedVehicleTest directionTest;
    directionTest.Initialize(settings);
    for (int i = 0; i < 180; ++i)
    {
        directionTest.Step(dt);
    }
    Tank::Physics::TankInput directionInput;
    directionInput.leftLeverX = -1.0f;
    directionInput.rightLeverX = -1.0f;
    directionTest.SetInput(directionInput);
    for (int i = 0; i < 12; ++i)
    {
        directionTest.Step(dt);
    }
    passed &= Check(BodyUpX(directionTest.State().bodyRotation) < -0.01f,
        "negative paired levers must start a negative local-Z roll toward -X");

    // Landing makes the next roll immediately eligible. This must not depend
    // on a visible post-landing slide.
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
    bool chainRequestedAtLanding = false;
    bool chainStarted = false;
    for (int i = 0; i < 480; ++i)
    {
        chainTest.Step(dt);
        const auto& chainState = chainTest.State();
        if (!chainRequestedAtLanding && chainState.rollChainAvailable)
        {
            chainInput.leftLeverX = 1.0f;
            chainInput.rightLeverX = 1.0f;
            chainTest.SetInput(chainInput);
            chainRequestedAtLanding = true;
            continue;
        }
        if (chainRequestedAtLanding)
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
            if (chainStarted)
            {
                break;
            }
        }
    }
    passed &= Check(chainRequestedAtLanding,
        "roll chain must become available at landing");
    passed &= Check(chainStarted,
        "a roll requested immediately after landing must start");

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
                Tank::Physics::RollingPhase::PoweredRoll &&
            BodyUpY(returnTest.State().bodyRotation) < 0.30f)
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
        "reverse lever pair must be accepted before the 90 degree decision");
    passed &= Check(returnDistance < 1.0f,
        "reverse input at 90 degrees must return near the starting position");
    passed &= Check(BodyUpY(returnTest.State().bodyRotation) > 0.95f &&
            returnTest.State().rollingPhase == Tank::Physics::RollingPhase::None,
        "reverse input must finish upright instead of remaining in BallisticRoll");
    passed &= Check(returnTest.State().lastRollingDecision ==
            Tank::Physics::RollingDecision::ReturnToStart &&
            returnTest.State().rollingDecisionCommandSign > 0.0f &&
            returnTest.State().rollingDecisionInputSign < 0.0f,
        "reverse decision diagnostics must record command and input signs");

    // Holding the same paired lever command across the 90 degree decision
    // must remain ContinueForward. It must never become a silent reverse.
    Tank::Physics::TrackedVehicleTest heldInputTest;
    heldInputTest.Initialize(settings);
    for (int i = 0; i < 180; ++i)
    {
        heldInputTest.Step(dt);
    }
    const Tank::Physics::Vec3 heldInputStart = heldInputTest.State().bodyPosition;
    Tank::Physics::TankInput heldInput;
    heldInput.leftLeverX = -1.0f;
    heldInput.rightLeverX = -1.0f;
    heldInputTest.SetInput(heldInput);
    for (int i = 0; i < 720; ++i)
    {
        heldInputTest.Step(dt);
    }
    const float heldInputDisplacementX =
        heldInputTest.State().bodyPosition.x - heldInputStart.x;
    passed &= Check(heldInputTest.State().lastRollingDecision ==
            Tank::Physics::RollingDecision::ContinueForward &&
            heldInputTest.State().rollingDecisionCommandSign < 0.0f &&
            heldInputTest.State().rollingDecisionInputSign < 0.0f,
        "same paired levers at 90 degrees must remain ContinueForward");
    passed &= Check(heldInputDisplacementX < -5.0f,
        "same paired levers must not reverse their roll travel before landing");

    // A new left request after a completed left roll must retain its sign;
    // MoveCompleted used to overwrite lastEvent and silently selected +1.
    Tank::Physics::TrackedVehicleTest sequentialLeftTest;
    sequentialLeftTest.Initialize(settings);
    for (int i = 0; i < 180; ++i)
    {
        sequentialLeftTest.Step(dt);
    }
    Tank::Physics::TankInput sequentialLeftInput;
    for (int rollIndex = 0; rollIndex < 2; ++rollIndex)
    {
        sequentialLeftInput.leftLeverX = -1.0f;
        sequentialLeftInput.rightLeverX = -1.0f;
        sequentialLeftTest.SetInput(sequentialLeftInput);
        sequentialLeftTest.Step(dt);
        sequentialLeftInput.leftLeverX = 0.0f;
        sequentialLeftInput.rightLeverX = 0.0f;
        sequentialLeftTest.SetInput(sequentialLeftInput);
        for (int i = 0; i < 720; ++i)
        {
            sequentialLeftTest.Step(dt);
        }
    }
    passed &= Check(sequentialLeftTest.State().rollingTraceRequestSign < 0.0f &&
            sequentialLeftTest.State().rollingTraceCommandSign < 0.0f,
        "each sequential left roll must latch a negative command sign");
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
    bool sawUnexpectedForwardReversal = false;
    float maximumRollDisplacementX = 0.0f;
    float cutoffDisplacementX = 0.0f;
    int framesToInverted = -1;
    int framesToSettling = -1;
    bool capturedLandingPosition = false;
    float landingDisplacementX = 0.0f;
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
        const bool forwardRollPhase =
            test.State().rollingPhase == Tank::Physics::RollingPhase::PoweredRoll ||
            test.State().rollingPhase == Tank::Physics::RollingPhase::Evaluating ||
            test.State().rollingPhase == Tank::Physics::RollingPhase::CommitRoll ||
            test.State().rollingPhase == Tank::Physics::RollingPhase::BallisticRoll;
        // +1 is converted to Jolt's negative local-Z angular velocity. A
        // sign flip before landing means another controller fought the
        // accepted ContinueForward roll.
        sawUnexpectedForwardReversal |= forwardRollPhase &&
            test.State().angularVelocity.z > 0.1f;
        if (framesToSettling < 0 && test.State().rollingPhase ==
            Tank::Physics::RollingPhase::Settling)
        {
            framesToSettling = i + 1;
        }
        const float currentDisplacementX =
            test.State().bodyPosition.x - startPosition.x;
        maximumRollDisplacementX = (std::max)(
            maximumRollDisplacementX,
            currentDisplacementX);
        if (test.State().rollingPhase == Tank::Physics::RollingPhase::Settling)
        {
            if (!capturedLandingPosition)
            {
                capturedLandingPosition = true;
                landingDisplacementX = currentDisplacementX;
            }
        }
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
    passed &= Check(state.lastRollingDecision ==
            Tank::Physics::RollingDecision::ContinueForward &&
            state.rollingDecisionCommandSign > 0.0f &&
            std::abs(state.rollingDecisionInputSign) < 0.001f,
        "forward decision diagnostics must record the neutral input");
    passed &= Check(sawBallisticRoll,
        "rolling must expose BallisticRoll phase");
    passed &= Check(!sawUnexpectedForwardReversal,
        "a ContinueForward roll must not reverse before landing");
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

    // A completed roll must not replay its old RollStarting request. Keep
    // both levers neutral for a further three seconds and require the phase
    // to stay clear throughout.
    bool restartedWhileNeutral = false;
    for (int i = 0; i < 180; ++i)
    {
        test.Step(dt);
        restartedWhileNeutral |= test.State().rollingPhase ==
            Tank::Physics::RollingPhase::PoweredRoll ||
            test.State().rollingPhase ==
                Tank::Physics::RollingPhase::Evaluating ||
            test.State().rollingPhase ==
                Tank::Physics::RollingPhase::CommitRoll ||
            test.State().rollingPhase ==
                Tank::Physics::RollingPhase::BallisticRoll;
    }
    passed &= Check(!restartedWhileNeutral,
        "neutral levers must not replay a completed roll");
    passed &= Check(lateralDistance > 5.0f && lateralDistance < 5.6f,
        "one roll must translate approximately one vehicle width");
    passed &= Check(capturedLandingPosition,
        "a completed roll must enter Settling after its landing transition");
    passed &= Check(displacementX > 5.0f,
        "positive lever sign must apply positive model-Z roll rotation");
    passed &= Check(maximumRollDisplacementX > 5.0f,
        "the tank must travel in its model-Z rotation fall direction");
    passed &= Check(cutoffDisplacementX > 0.5f,
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
    passed &= Check(secondRollDisplacementX > 4.5f,
        "an inverted roll must move toward the tread opposite the raised tread");
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
            << " maximumRollDisplacementX=" << maximumRollDisplacementX
            << " landingDisplacementX=" << landingDisplacementX
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
