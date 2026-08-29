#include "RollingRecoveryController.h"

#include <algorithm>
#include <cmath>

namespace Tank::Physics
{
    RollingRecoveryController::RollingRecoveryController(
        const RollingRecoverySettings& settings)
        : m_settings(settings)
    {
        m_settings.torqueNm = (std::max)(
            std::isfinite(settings.torqueNm) ? settings.torqueNm : 0.0f,
            0.0f);
        m_settings.dampingNms = (std::max)(
            std::isfinite(settings.dampingNms) ? settings.dampingNms : 0.0f,
            0.0f);
        m_settings.maximumTorqueNm = (std::max)(
            std::isfinite(settings.maximumTorqueNm)
                ? settings.maximumTorqueNm : 0.0f,
            0.0f);
    }

    float RollingRecoveryController::ComputeTorque(
        const RollingRecoveryInput& input) const
    {
        if (!input.blocked || !std::isfinite(input.rollError) ||
            !std::isfinite(input.rollAngularVelocityRadiansPerSecond))
        {
            return 0.0f;
        }
        const float rawTorque = input.rollError * m_settings.torqueNm -
            input.rollAngularVelocityRadiansPerSecond * m_settings.dampingNms;
        return std::clamp(
            rawTorque,
            -m_settings.maximumTorqueNm,
            m_settings.maximumTorqueNm);
    }
}
