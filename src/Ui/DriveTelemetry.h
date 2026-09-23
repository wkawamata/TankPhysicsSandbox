#pragma once

#include <algorithm>
#include <cstddef>
#include <cmath>
#include <vector>

namespace Ui
{
    enum class DriveSpeedState
    {
        Acceleration,
        NaturalBrake,
        Brake,
        Coast,
    };

    inline DriveSpeedState ClassifyDriveSpeedState(float forwardInput, float brakeInput)
    {
        constexpr float kInputEpsilon = 0.001f;
        constexpr float kFullBrakeThreshold = 0.999f;
        if (brakeInput >= kFullBrakeThreshold)
        {
            return DriveSpeedState::Brake;
        }
        if (brakeInput > kInputEpsilon)
        {
            return DriveSpeedState::NaturalBrake;
        }
        if (std::abs(forwardInput) > kInputEpsilon)
        {
            return DriveSpeedState::Acceleration;
        }
        return DriveSpeedState::Coast;
    }

    struct DriveSpeedSample
    {
        int stepIndex = 0;
        float timeSeconds = 0.0f;
        float speedMetersPerSecond = 0.0f;
        DriveSpeedState state = DriveSpeedState::Coast;
    };

    struct DriveSpeedGraphSettings
    {
        float historyDurationSeconds = 15.0f;
        float maximumSpeedMetersPerSecond = 60.0f;
    };

    class DriveSpeedHistory
    {
    public:
        void Update(
            int stepIndex,
            float timeSeconds,
            float speedMetersPerSecond,
            DriveSpeedState state,
            bool stopped)
        {
            if (stepIndex < m_previousStepIndex)
            {
                m_samples.clear();
                m_stopped = false;
            }
            if (stopped)
            {
                m_stopped = true;
                m_previousStepIndex = stepIndex;
                return;
            }
            if (m_stopped)
            {
                m_samples.clear();
                m_stopped = false;
            }
            if (stepIndex != m_previousStepIndex)
            {
                m_samples.push_back({ stepIndex, timeSeconds, speedMetersPerSecond, state });
                constexpr float kMaximumHistoryDurationSeconds = 60.0f;
                while (m_samples.size() > 1 &&
                    m_samples.front().timeSeconds <
                        timeSeconds - kMaximumHistoryDurationSeconds)
                {
                    m_samples.erase(m_samples.begin());
                }
            }
            m_previousStepIndex = stepIndex;
        }

        const std::vector<DriveSpeedSample>& Samples() const { return m_samples; }

    private:
        int m_previousStepIndex = -1;
        bool m_stopped = false;
        std::vector<DriveSpeedSample> m_samples;
    };
}
