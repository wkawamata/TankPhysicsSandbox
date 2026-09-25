#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
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
        float analogLeftTrack = 0.0f;
        float analogRightTrack = 0.0f;
        float forwardInput = 0.0f;
        float leftRatio = 0.0f;
        float rightRatio = 0.0f;
        float brakeInput = 0.0f;
        float engineRpm = 0.0f;
        float clutchFriction = 0.0f;
        float leftTrackAngularVelocityRadians = 0.0f;
        float rightTrackAngularVelocityRadians = 0.0f;
        float leftTrackDriveTorqueNm = 0.0f;
        float rightTrackDriveTorqueNm = 0.0f;
        float yawSpeedDegrees = 0.0f;
        int gear = 0;
        int wheelContactCount = 0;
        bool switchingGear = false;
        bool stopped = false;
        float rollLeftLever = 0.0f;
        float rollRightLever = 0.0f;
        bool rollInputArmed = true;
        float rollPendingSign = 0.0f;
        int rollPhase = 0;
        int rollStopGate = 0;
        int rollRejectReason = 0;
        float stopProgress = 0.0f;
        float angularSpeed = 0.0f;
        float rollAngleDegrees = 0.0f;
        float rollAccumulatedAngleDegrees = 0.0f;
        float rollSpeedDegrees = 0.0f;
        float rollTravelMeters = 0.0f;
        float rollTargetMeters = 0.0f;
        float maximumSlip = 0.0f;
        float maximumSuspensionSpeed = 0.0f;
        float rollPrimaryTorqueNm = 0.0f;
        float rollApproachDampingTorqueNm = 0.0f;
        float rollCommitTorqueNm = 0.0f;
        float rollAirBrakeTorqueNm = 0.0f;
        float rollStabilizationTorqueNm = 0.0f;
        float rollControllerTorqueSumNm = 0.0f;
        std::uint64_t rollRunId = 0;
    };

    struct DriveTelemetryInput
    {
        DriveSpeedSample sample = {};
        bool stopped = false;
    };

    struct DriveSpeedGraphSettings
    {
        float historyDurationSeconds = 15.0f;
        float maximumSpeedMetersPerSecond = 60.0f;
    };

    class DriveSpeedHistory
    {
    public:
        void Update(const DriveTelemetryInput& telemetry)
        {
            const DriveSpeedSample& sample = telemetry.sample;
            const int stepIndex = sample.stepIndex;
            const float timeSeconds = sample.timeSeconds;
            if (stepIndex < m_previousStepIndex)
            {
                m_samples.clear();
            }
            if (stepIndex != m_previousStepIndex)
            {
                m_samples.push_back(sample);
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
        std::vector<DriveSpeedSample> m_samples;
    };
}
