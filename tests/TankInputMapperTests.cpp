#include "Input/TankInputMapper.h"

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
            std::cerr << "FAIL TankInputMapper: " << message << "\n";
        }

        return condition;
    }
}

int main()
{
    bool passed = true;

    Tank::Input::GamepadState disconnected;
    const Tank::Physics::TankInput disconnectedInput =
        Tank::Input::MapGamepadToTankInput(disconnected);
    passed &= Check(NearlyEqual(disconnectedInput.throttle, 0.0f), "disconnected throttle must be zero");
    passed &= Check(NearlyEqual(disconnectedInput.steering, 0.0f), "disconnected steering must be zero");

    Tank::Input::GamepadState centered;
    centered.connected = true;
    centered.leftStickX = 0.1f;
    centered.leftStickY = 0.1f;
    const Tank::Physics::TankInput centeredInput =
        Tank::Input::MapGamepadToTankInput(centered);
    passed &= Check(NearlyEqual(centeredInput.throttle, 0.0f), "deadzone throttle must be zero");
    passed &= Check(NearlyEqual(centeredInput.steering, 0.0f), "deadzone steering must be zero");

    Tank::Input::GamepadState forward;
    forward.connected = true;
    forward.leftStickY = 1.0f;
    const Tank::Physics::TankInput forwardInput =
        Tank::Input::MapGamepadToTankInput(forward);
    passed &= Check(NearlyEqual(forwardInput.throttle, 1.0f), "full forward must map to full throttle");
    passed &= Check(NearlyEqual(forwardInput.leftTrack, 1.0f), "straight input must use default left ratio");
    passed &= Check(NearlyEqual(forwardInput.rightTrack, 1.0f), "straight input must use default right ratio");

    Tank::Input::GamepadState movingLeft;
    movingLeft.connected = true;
    movingLeft.leftStickX = -0.5f;
    movingLeft.leftStickY = 1.0f;
    const Tank::Physics::TankInput movingLeftInput =
        Tank::Input::MapGamepadToTankInput(movingLeft);
    passed &= Check(
        movingLeftInput.leftTrack < movingLeftInput.rightTrack,
        "left steering must slow the left track");

    Tank::Input::GamepadState pivotLeft;
    pivotLeft.connected = true;
    pivotLeft.leftStickX = -1.0f;
    const Tank::Physics::TankInput pivotLeftInput =
        Tank::Input::MapGamepadToTankInput(pivotLeft);
    passed &= Check(NearlyEqual(pivotLeftInput.throttle, 1.0f), "full pivot must use full throttle");
    passed &= Check(NearlyEqual(pivotLeftInput.leftTrack, -1.0f), "left pivot must reverse the left track");
    passed &= Check(NearlyEqual(pivotLeftInput.rightTrack, 1.0f), "left pivot must advance the right track");

    Tank::Input::GamepadState braking;
    braking.connected = true;
    braking.brakePressed = true;
    const Tank::Physics::TankInput brakingInput =
        Tank::Input::MapGamepadToTankInput(braking);
    passed &= Check(brakingInput.brake, "brake button must be preserved");

    if (!passed)
    {
        return 1;
    }

    std::cout << "PASS TankInputMapper\n";
    return 0;
}
