#pragma once

#include <algorithm>
#include <array>
#include <cmath>

namespace Tank::Physics
{
    // Multipliers at x2, interpolated exponentially so x1 is always unchanged.
    struct RollingSpeedTuning
    {
        float driveTorque = 4.0f;
        float approachDamping = 2.0f;
        float commitTorque = 4.0f;
        float airBrakeTorque = 4.0f;
        float stabilizationTorque = 4.0f;
        float stabilizationDamping = 2.0f;
        float travelForce = 4.0f;
        float travelDamping = 2.0f;
        float forceLimit = 1.0f;
        float gravity = 1.0f;
        float suspensionFrequency = 1.0f;
        float commitDuration = 1.0f;
        bool operator==(const RollingSpeedTuning&) const = default;
    };

    struct RollingSpeedCoefficient
    {
        const char* key;
        const char* label;
        float RollingSpeedTuning::* member;
        float minimum;
        float maximum;
    };

    inline constexpr std::array<RollingSpeedCoefficient, 12> kRollingSpeedCoefficients = {{
        { "driveTorque", "Drive torque", &RollingSpeedTuning::driveTorque, 0.25f, 12.0f },
        { "approachDamping", "Approach damping", &RollingSpeedTuning::approachDamping, 0.25f, 8.0f },
        { "commitTorque", "Commit torque", &RollingSpeedTuning::commitTorque, 0.25f, 12.0f },
        { "airBrakeTorque", "Air brake torque", &RollingSpeedTuning::airBrakeTorque, 0.25f, 12.0f },
        { "stabilizationTorque", "Stabilization torque", &RollingSpeedTuning::stabilizationTorque, 0.25f, 12.0f },
        { "stabilizationDamping", "Stabilization damping", &RollingSpeedTuning::stabilizationDamping, 0.25f, 8.0f },
        { "travelForce", "Travel force (P/I)", &RollingSpeedTuning::travelForce, 0.25f, 12.0f },
        { "travelDamping", "Travel damping", &RollingSpeedTuning::travelDamping, 0.25f, 8.0f },
        { "forceLimit", "Travel force limit", &RollingSpeedTuning::forceLimit, 0.25f, 12.0f },
        { "gravity", "Gravity", &RollingSpeedTuning::gravity, 0.25f, 8.0f },
        { "suspensionFrequency", "Suspension frequency", &RollingSpeedTuning::suspensionFrequency, 0.5f, 4.0f },
        { "commitDuration", "Commit duration", &RollingSpeedTuning::commitDuration, 0.125f, 2.0f },
    }};

    inline RollingSpeedTuning SanitizeRollingSpeedTuning(RollingSpeedTuning tuning)
    {
        const RollingSpeedTuning defaults;
        for (const auto& coefficient : kRollingSpeedCoefficients)
        {
            float& value = tuning.*(coefficient.member);
            value = std::isfinite(value)
                ? std::clamp(value, coefficient.minimum, coefficient.maximum)
                : defaults.*(coefficient.member);
        }
        return tuning;
    }

    inline float RollingSpeedScale(float multiplierAtTwo, float speed)
    {
        return speed == 1.0f ? 1.0f : std::pow(multiplierAtTwo, std::log2(speed));
    }
}
