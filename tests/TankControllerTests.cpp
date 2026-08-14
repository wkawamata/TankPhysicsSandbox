#include "Physics/TankController.h"
#include "Physics/PhysicsWorld.h"

#include <cmath>
#include <iostream>

namespace
{
    bool NearlyEqual(float left, float right)
    {
        return std::abs(left - right) < 0.0001f;
    }

    bool Check(bool condition, const char* message)
    {
        if (!condition)
        {
            std::cerr << "FAIL TankController: " << message << "\n";
        }

        return condition;
    }
}

int main()
{
    Tank::Physics::PhysicsWorld world;
    world.Initialize();
    Tank::Physics::TankController controller;
    controller.Initialize(world);

    const Tank::Physics::TankState& initialState = controller.State();
    bool passed = true;
    passed &= Check(initialState.stepIndex == 0, "initial step must be zero");
    passed &= Check(NearlyEqual(initialState.timeSeconds, 0.0f), "initial time must be zero");
    passed &= Check(NearlyEqual(initialState.body.rotation.w, 1.0f), "initial rotation must be identity");
    passed &= Check(NearlyEqual(controller.Input().leftTrack, 1.0f),
        "left track ratio must default to one");
    passed &= Check(NearlyEqual(controller.Input().rightTrack, 1.0f),
        "right track ratio must default to one");
    passed &= Check(NearlyEqual(controller.Settings().chassisMassKg, 4000.0f),
        "chassis mass must default to 4000 kg");
    passed &= Check(!controller.Settings().rollingInputEnabled,
        "rolling input must be disabled by default");
    passed &= Check(NearlyEqual(controller.Settings().rollTorqueNm, 120000.0f),
        "roll torque must default to 120000 N m");
    passed &= Check(NearlyEqual(controller.Settings().rollDistanceM, 2.4f),
        "roll distance must default to one vehicle width");
    passed &= Check(NearlyEqual(controller.Settings().rollTorqueCutoffDegrees, 90.0f),
        "roll torque cutoff must default to 90 degrees");
    passed &= Check(NearlyEqual(controller.Settings().rollStabilizationTorqueNm, 30000.0f),
        "roll stabilization torque must use its default");
    passed &= Check(NearlyEqual(controller.Settings().rollStabilizationDampingNms, 10000.0f),
        "roll stabilization damping must use its default");
    passed &= Check(NearlyEqual(controller.Settings().trackWidthM, 0.3f),
        "track width must default to 0.3 m");
    passed &= Check(NearlyEqual(controller.Settings().trackSpacingM, 2.4f),
        "track spacing must default to 2.4 m");
    passed &= Check(NearlyEqual(controller.Settings().rideHeightScale, 0.8f),
        "ride height must default to 80 percent");
    passed &= Check(NearlyEqual(controller.Settings().suspensionFrequencyHz, 1.0f),
        "suspension frequency must preserve current behavior");
    passed &= Check(NearlyEqual(controller.Settings().suspensionDamping, 0.5f),
        "suspension damping must preserve the Jolt default");
    passed &= Check(NearlyEqual(controller.Settings().twoRoadWheelOffsetM, 0.67f),
        "two road wheel offset must preserve its default");
    passed &= Check(NearlyEqual(controller.Settings().endWheelOffsetM, 0.0f),
        "end wheel offset must default to the chassis ends");
    passed &= Check(NearlyEqual(controller.Settings().wheelHorizontalOffsetM, 0.0f),
        "wheel horizontal offset must default to zero");
    passed &= Check(NearlyEqual(controller.Settings().threeRoadWheelOffsetM, 1.0f),
        "three road wheel offset must preserve its default");
    passed &= Check(NearlyEqual(controller.Settings().stationaryTurnLeftTraction, 1.0f),
        "stationary turn left traction must default to one");
    passed &= Check(NearlyEqual(controller.Settings().stationaryTurnInnerTrackRatio, 0.0f),
        "stationary turn inner track ratio must default to zero");
    passed &= Check(NearlyEqual(controller.Settings().pivotTurnRightTraction, 1.0f),
        "pivot turn right traction must default to one");

    Tank::Physics::TankInput input;
    input.throttle = 2.0f;
    input.steering = -2.0f;
    input.leftTrack = 1.5f;
    input.rightTrack = -1.5f;
    input.roll = 2.0f;
    input.brakeAmount = 2.0f;
    input.brake = true;
    controller.SetInput(input);

    const Tank::Physics::TankInput& clampedInput = controller.Input();
    passed &= Check(NearlyEqual(clampedInput.throttle, 1.0f), "throttle must be clamped");
    passed &= Check(NearlyEqual(clampedInput.steering, -1.0f), "steering must be clamped");
    passed &= Check(NearlyEqual(clampedInput.leftTrack, 1.0f), "left track must be clamped");
    passed &= Check(NearlyEqual(clampedInput.rightTrack, -1.0f), "right track must be clamped");
    passed &= Check(NearlyEqual(clampedInput.roll, 0.0f),
        "roll must be suppressed by default");
    passed &= Check(NearlyEqual(clampedInput.brakeAmount, 1.0f),
        "brake amount must be clamped");
    passed &= Check(clampedInput.brake, "brake must be preserved");

    input.leftTrack = 0.0f;
    input.rightTrack = 1.0f;
    input.brakeAmount = 0.2f;
    input.brake = false;
    controller.SetInput(input);
    passed &= Check(NearlyEqual(controller.Input().leftTrack, 0.0f),
        "zero track ratio must be preserved for stationary turns");

    controller.PreStep();
    const Tank::Physics::TrackedDriverInput& driverInput = controller.DriverInput();
    passed &= Check(NearlyEqual(driverInput.forward, 1.0f),
        "driver forward input must match the clamped throttle");
    passed &= Check(NearlyEqual(driverInput.leftRatio, 0.001f),
        "zero left track ratio must use Jolt's minimum positive ratio");
    passed &= Check(NearlyEqual(driverInput.rightRatio, 1.0f),
        "driver right ratio must match the track input");
    passed &= Check(NearlyEqual(driverInput.brake, 0.2f),
        "driver brake input must preserve a partial brake amount");
    controller.PostStep(1.0f / 60.0f);
    passed &= Check(controller.State().stepIndex == 1, "positive step must advance the index");
    passed &= Check(controller.State().timeSeconds > 0.0f, "positive step must advance time");

    controller.PostStep(0.0f);
    passed &= Check(controller.State().stepIndex == 1, "non-positive step must be ignored");

    Tank::Physics::TankSettings disabledRollSettings;
    disabledRollSettings.rollingInputEnabled = false;
    Tank::Physics::TankController disabledRollController;
    disabledRollController.Initialize(world, disabledRollSettings);
    Tank::Physics::TankInput disabledRollInput;
    disabledRollInput.roll = 1.0f;
    disabledRollController.SetInput(disabledRollInput);
    passed &= Check(NearlyEqual(disabledRollController.Input().roll, 0.0f),
        "disabled rolling input must suppress roll commands");

    Tank::Physics::TankSettings tractionSettings;
    tractionSettings.stationaryTurnLeftTraction = 0.4f;
    tractionSettings.stationaryTurnRightTraction = 0.6f;
    tractionSettings.stationaryTurnInnerTrackRatio = 0.5f;
    tractionSettings.pivotTurnLeftTraction = 0.7f;
    tractionSettings.pivotTurnRightTraction = 0.8f;
    Tank::Physics::TankController tractionController;
    tractionController.Initialize(world, tractionSettings);

    Tank::Physics::TankSettings wideTrackSettings;
    wideTrackSettings.trackSpacingM = 6.0f;
    wideTrackSettings.trackWidthM = 1.0f;
    Tank::Physics::TankController wideTrackController;
    wideTrackController.Initialize(world, wideTrackSettings);
    passed &= Check(NearlyEqual(wideTrackController.Settings().trackSpacingM, 6.0f),
        "track spacing must support the expanded outer limit");
    passed &= Check(NearlyEqual(wideTrackController.Settings().trackWidthM, 1.0f),
        "track width must support the expanded upper limit");

    Tank::Physics::TankInput tractionInput;
    tractionInput.throttle = 1.0f;
    tractionInput.leftTrack = 0.0f;
    tractionInput.rightTrack = 1.0f;
    tractionController.SetInput(tractionInput);
    tractionController.PreStep();
    passed &= Check(
        NearlyEqual(tractionController.DriverInput().leftRatio, 0.2f),
        "stationary inner track must use its ratio and traction coefficient");
    passed &= Check(
        NearlyEqual(tractionController.DriverInput().rightRatio, 0.6f),
        "stationary driven track must use its traction coefficient");

    tractionInput.leftTrack = -1.0f;
    tractionInput.rightTrack = 1.0f;
    tractionController.SetInput(tractionInput);
    tractionController.PreStep();
    passed &= Check(
        NearlyEqual(tractionController.DriverInput().leftRatio, -0.7f),
        "pivot left track must use its traction coefficient");
    passed &= Check(
        NearlyEqual(tractionController.DriverInput().rightRatio, 0.8f),
        "pivot right track must use its traction coefficient");

    Tank::Physics::TankSettings suspensionLimitSettings;
    suspensionLimitSettings.suspensionFrequencyHz = 50.0f;
    suspensionLimitSettings.suspensionDamping = -1.0f;
    Tank::Physics::TankController suspensionLimitController;
    suspensionLimitController.Initialize(world, suspensionLimitSettings);
    passed &= Check(
        NearlyEqual(suspensionLimitController.Settings().suspensionFrequencyHz, 10.0f),
        "suspension frequency must be clamped before Jolt setup");
    passed &= Check(
        NearlyEqual(suspensionLimitController.Settings().suspensionDamping, 0.0f),
        "suspension damping must be clamped before Jolt setup");

    if (!passed)
    {
        return 1;
    }

    std::cout << "PASS TankController boundary\n";
    return 0;
}
