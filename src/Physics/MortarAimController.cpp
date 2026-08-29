#include "MortarAimController.h"

#include <algorithm>
#include <cmath>

namespace Tank::Physics
{
    MortarAimController::MortarAimController(
        const MortarAimSettings& settings)
        : m_settings(settings)
    {
        Reset();
    }

    MortarAimSnapshot MortarAimController::Update(float deltaTimeSeconds)
    {
        if (std::isfinite(deltaTimeSeconds) && deltaTimeSeconds > 0.0f)
        {
            m_snapshot.angleDegrees = std::min(
                m_settings.maximumAngleDegrees,
                m_snapshot.angleDegrees +
                    m_settings.angleRateDegreesPerSecond * deltaTimeSeconds);
        }

        const float span = m_settings.maximumAngleDegrees -
            m_settings.minimumFireAngleDegrees;
        const float normalized = span > 0.0f
            ? std::clamp((m_snapshot.angleDegrees -
                m_settings.minimumFireAngleDegrees) / span, 0.0f, 1.0f)
            : 1.0f;
        m_snapshot.rangeMeters = m_settings.minimumRangeMeters +
            (m_settings.maximumRangeMeters - m_settings.minimumRangeMeters) *
            normalized;
        m_snapshot.canFire = m_snapshot.angleDegrees >=
            m_settings.minimumFireAngleDegrees;
        m_snapshot.atMaximum = m_snapshot.angleDegrees >=
            m_settings.maximumAngleDegrees;
        return m_snapshot;
    }

    void MortarAimController::Reset()
    {
        m_snapshot = {};
        m_snapshot.rangeMeters = m_settings.minimumRangeMeters;
    }
}
