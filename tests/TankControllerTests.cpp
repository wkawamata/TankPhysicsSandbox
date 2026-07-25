#include "Physics/TankController.h"

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
    Tank::Physics::TankController controller;
    controller.Initialize();

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
    passed &= Check(NearlyEqual(controller.Settings().rideHeightScale, 0.8f),
        "ride height must default to 80 percent");

    Tank::Physics::TankInput input;
    input.throttle = 2.0f;
    input.steering = -2.0f;
    input.leftTrack = 1.5f;
    input.rightTrack = -1.5f;
    input.roll = 2.0f;
    input.brake = true;
    controller.SetInput(input);

    const Tank::Physics::TankInput& clampedInput = controller.Input();
    passed &= Check(NearlyEqual(clampedInput.throttle, 1.0f), "throttle must be clamped");
    passed &= Check(NearlyEqual(clampedInput.steering, -1.0f), "steering must be clamped");
    passed &= Check(NearlyEqual(clampedInput.leftTrack, 1.0f), "left track must be clamped");
    passed &= Check(NearlyEqual(clampedInput.rightTrack, -1.0f), "right track must be clamped");
    passed &= Check(NearlyEqual(clampedInput.roll, 1.0f), "roll must be clamped");
    passed &= Check(clampedInput.brake, "brake must be preserved");

    input.leftTrack = 0.0f;
    input.rightTrack = 1.0f;
    controller.SetInput(input);
    passed &= Check(NearlyEqual(controller.Input().leftTrack, 0.0f),
        "zero track ratio must be preserved for stationary turns");

    controller.PreStep();
    controller.PostStep(1.0f / 60.0f);
    passed &= Check(controller.State().stepIndex == 1, "positive step must advance the index");
    passed &= Check(controller.State().timeSeconds > 0.0f, "positive step must advance time");

    controller.PostStep(0.0f);
    passed &= Check(controller.State().stepIndex == 1, "non-positive step must be ignored");

    if (!passed)
    {
        return 1;
    }

    std::cout << "PASS TankController boundary\n";
    return 0;
}
