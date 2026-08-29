#pragma once

#include "PhysicsTypes.h"

namespace Tank::Physics
{
    struct RollingRecoverySettings
    {
        float torqueNm = 30000.0f;
        float dampingNms = 10000.0f;
        float maximumTorqueNm = 45000.0f;
    };

    struct RollingRecoveryInput
    {
        float rollError = 0.0f;
        float rollAngularVelocityRadiansPerSecond = 0.0f;
        bool blocked = false;
    };

    class RollingRecoveryController
    {
    public:
        explicit RollingRecoveryController(
            const RollingRecoverySettings& settings = {});
        float ComputeTorque(const RollingRecoveryInput& input) const;

    private:
        RollingRecoverySettings m_settings = {};
    };
}
