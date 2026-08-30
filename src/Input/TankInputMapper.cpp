#include "TankInputMapper.h"

#include <algorithm>
#include <cmath>

namespace Tank::Input
{
    namespace
    {
        struct StickState
        {
            float x = 0.0f;
            float y = 0.0f;
        };

        StickState ApplyCircularDeadzone(float x, float y, float deadzone)
        {
            x = std::clamp(x, -1.0f, 1.0f);
            y = std::clamp(y, -1.0f, 1.0f);
            deadzone = std::clamp(deadzone, 0.0f, 0.99f);

            const float magnitude = std::sqrt(x * x + y * y);
            if (magnitude <= deadzone)
            {
                return {};
            }

            const float clampedMagnitude = std::min(magnitude, 1.0f);
            const float normalizedMagnitude = (clampedMagnitude - deadzone) / (1.0f - deadzone);
            const float scale = normalizedMagnitude / magnitude;
            return {x * scale, y * scale};
        }
    }

    Physics::TankInput MapGamepadToTankInput(
        const GamepadState& state,
        const TankInputMappingSettings& settings)
    {
        Physics::TankInput input;
        if (!state.connected)
        {
            return input;
        }

        const StickState stick =
            ApplyCircularDeadzone(state.leftStickX, state.leftStickY, settings.stickDeadzone);
        input.throttle = stick.y;
        input.steering = stick.x;
        input.brake = state.brakePressed;
        if (settings.leftLeverAxis < state.rawAxes.size())
            input.leftLeverX = std::clamp(state.rawAxes[settings.leftLeverAxis], -1.0f, 1.0f);
        if (settings.rightLeverAxis < state.rawAxes.size())
            input.rightLeverX = std::clamp(state.rawAxes[settings.rightLeverAxis], -1.0f, 1.0f);

        const float pivotThreshold = std::clamp(settings.pivotThreshold, 0.0f, 1.0f);
        if (std::abs(input.throttle) <= pivotThreshold && input.steering != 0.0f)
        {
            input.throttle =
                std::clamp(std::abs(input.steering) * settings.pivotSpeed, 0.0f, 1.0f);
            input.leftTrack = input.steering < 0.0f ? -1.0f : 1.0f;
            input.rightTrack = -input.leftTrack;
            return input;
        }

        const float reduction =
            std::clamp(std::abs(input.steering) * settings.steeringTrackReduction, 0.0f, 1.0f);
        if (input.steering < 0.0f)
        {
            input.leftTrack = 1.0f - reduction;
            input.rightTrack = 1.0f;
        }
        else if (input.steering > 0.0f)
        {
            input.leftTrack = 1.0f;
            input.rightTrack = 1.0f - reduction;
        }

        return input;
    }
}
