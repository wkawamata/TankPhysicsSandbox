#include "RollingObstructionDetector.h"

#include <algorithm>
#include <cmath>

namespace Tank::Physics
{
    RollingObstructionDetector::RollingObstructionDetector(
        const RollingObstructionSettings& settings)
        : m_settings(settings)
    {
        m_settings.confirmSeconds = (std::max)(settings.confirmSeconds, 0.01f);
    }

    void RollingObstructionDetector::Reset()
    {
        m_state = RollingObstructionState::Clear;
        m_previousProgressDegrees = 0.0f;
        m_suspectTimeSeconds = 0.0f;
    }

    RollingObstructionState RollingObstructionDetector::Update(
        const RollingObstructionInput& input,
        float deltaTimeSeconds)
    {
        const float dt = std::isfinite(deltaTimeSeconds) &&
            deltaTimeSeconds > 0.0f ? deltaTimeSeconds : 0.0f;
        const float progressRate = std::abs(
            input.rotationProgressDegrees - m_previousProgressDegrees) /
            (dt > 0.0f ? dt : 1.0f);
        m_previousProgressDegrees = input.rotationProgressDegrees;
        const bool stalled = progressRate <
            m_settings.minimumProgressDegreesPerSecond;
        const bool constrained =
            std::abs(input.lateralSpeedMetersPerSecond) <=
                m_settings.maximumLateralSpeedMetersPerSecond &&
            input.contactImpulseNewtonSeconds >=
                m_settings.minimumContactImpulseNewtonSeconds &&
            input.controlSaturation >= m_settings.minimumControlSaturation;
        if (stalled && constrained)
        {
            m_suspectTimeSeconds += dt;
            m_state = m_suspectTimeSeconds >= m_settings.confirmSeconds
                ? RollingObstructionState::Blocked
                : RollingObstructionState::Suspected;
        }
        else
        {
            m_suspectTimeSeconds = 0.0f;
            m_state = RollingObstructionState::Clear;
        }
        return m_state;
    }
}
