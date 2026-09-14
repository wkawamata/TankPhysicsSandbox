#include "Physics/SpecialActionRecognizer.h"
#include "Physics/SpecialMoveStateMachine.h"
#include "Physics/TrackedVehicleTest.h"

#include <iostream>

namespace
{
    bool Check(bool condition, const char* message)
    {
        if (!condition)
        {
            std::cerr << "FAIL SpecialMove integration: " << message << "\n";
        }
        return condition;
    }
}

int main()
{
    using namespace Tank::Physics;
    constexpr float deltaTimeSeconds = 1.0f / 60.0f;
    bool passed = true;

    TrackedVehicleTest vehicle;
    vehicle.Initialize();
    for (int step = 0; step < 180; ++step)
    {
        vehicle.Step(deltaTimeSeconds);
    }
    passed &= Check(vehicle.State().mobility.state == MobilityState::Stopped,
        "settled physics vehicle must be Stopped");

    SpecialActionRecognizer recognizer;
    SpecialMoveStateMachine specialMoves;
    const SpecialAction action = recognizer.Update({ -0.8f, 0.8f });
    passed &= Check(action == SpecialAction::MortarRequested,
        "outward input must produce mortar request");
    passed &= Check(
        specialMoves.Update(SpecialMoveEvent::MortarRequested,
            vehicle.State().mobility.state == MobilityState::Stopped).state ==
            SpecialMoveState::MortarStarting,
        "Stopped physics state must accept mortar request");

    TankInput mortarInput;
    mortarInput.leftLeverX = -1.0f;
    mortarInput.rightLeverX = 1.0f;
    vehicle.SetInput(mortarInput);
    vehicle.Step(deltaTimeSeconds);
    passed &= Check(
        vehicle.State().specialMove.state == SpecialMoveState::MortarStarting,
        "outward TankInput must enter MortarStarting");
    for (int step = 0; step < 120; ++step)
    {
        vehicle.Step(deltaTimeSeconds);
    }
    passed &= Check(
        vehicle.State().specialMove.state == SpecialMoveState::MortarAiming &&
            vehicle.State().mortarAim.canFire,
        "mortar elevation must reach MortarAiming");
    const auto& raisedRotation = vehicle.State().bodyRotation;
    const float raisedForwardY = 2.0f * (
        raisedRotation.y * raisedRotation.z -
        raisedRotation.w * raisedRotation.x);
    passed &= Check(raisedForwardY > 0.20f,
        "held mortar input must physically raise the tank forward end");

    for (int step = 0; step < 120; ++step)
    {
        vehicle.Step(deltaTimeSeconds);
    }
    passed &= Check(
        vehicle.State().mortarAim.atMaximum &&
            vehicle.State().mortarAim.angleDegrees == 40.0f,
        "held mortar input must stop at its 40 degree maximum");

    vehicle.SetInput({});
    for (int step = 0; step < 240; ++step)
    {
        vehicle.Step(deltaTimeSeconds);
    }
    passed &= Check(
        vehicle.State().specialMove.state == SpecialMoveState::Idle &&
            vehicle.State().mortarAim.angleDegrees == 0.0f,
        "releasing mortar input must lower the stance and return to Idle");
    const auto& loweredRotation = vehicle.State().bodyRotation;
    const float loweredForwardY = 2.0f * (
        loweredRotation.y * loweredRotation.z -
        loweredRotation.w * loweredRotation.x);
    passed &= Check(std::abs(loweredForwardY) < 0.08f,
        "released mortar input must physically lower the tank");

    TankInput driveInput;
    driveInput.throttle = 1.0f;
    vehicle.SetInput(driveInput);
    vehicle.Step(deltaTimeSeconds);
    passed &= Check(vehicle.State().mobility.state == MobilityState::Moving,
        "drive request must move physics state to Moving");
    SpecialActionRecognizer movingRecognizer;
    SpecialMoveStateMachine movingSpecialMoves;
    const SpecialAction rollAction = movingRecognizer.Update({ -0.8f, -0.8f });
    passed &= Check(rollAction == SpecialAction::RollLeftRequested,
        "same-direction input must produce roll request");
    const SpecialMoveStateSnapshot& rejected = movingSpecialMoves.Update(
        SpecialMoveEvent::RollLeftRequested,
        vehicle.State().mobility.state == MobilityState::Stopped);
    passed &= Check(rejected.state == SpecialMoveState::Idle &&
            rejected.lastRejectReason ==
                SpecialMoveRejectReason::MobilityNotStopped,
        "Moving physics state must reject roll request");

    passed &= Check(recognizer.Update({ -0.8f, -0.8f }) == SpecialAction::None,
        "held roll input must not repeat in integration path");
    passed &= Check(recognizer.Update({ 0.0f, 0.0f }) == SpecialAction::None,
        "neutral input must re-arm integration path");
    passed &= Check(recognizer.Update({ -0.8f, -0.8f }) ==
            SpecialAction::RollLeftRequested,
        "roll input must work again after neutral re-arm");

    if (!passed)
    {
        return 1;
    }
    std::cout << "PASS SpecialMove integration\n";
    return 0;
}
