#pragma once

namespace Tank::Physics
{
    struct RollingObstructionInput
    {
        float rotationProgressDegrees = 0.0f;
        float lateralSpeedMetersPerSecond = 0.0f;
        float contactImpulseNewtonSeconds = 0.0f;
        float controlSaturation = 0.0f;
    };

    enum class RollingObstructionState
    {
        Clear,
        Suspected,
        Blocked
    };

    struct RollingObstructionSettings
    {
        float minimumProgressDegreesPerSecond = 2.0f;
        float maximumLateralSpeedMetersPerSecond = 0.10f;
        float minimumContactImpulseNewtonSeconds = 1000.0f;
        float minimumControlSaturation = 0.95f;
        float confirmSeconds = 0.20f;
    };

    class RollingObstructionDetector
    {
    public:
        explicit RollingObstructionDetector(
            const RollingObstructionSettings& settings = {});
        void Reset();
        RollingObstructionState Update(
            const RollingObstructionInput& input,
            float deltaTimeSeconds);
        RollingObstructionState State() const { return m_state; }

    private:
        RollingObstructionSettings m_settings = {};
        RollingObstructionState m_state = RollingObstructionState::Clear;
        float m_previousProgressDegrees = 0.0f;
        float m_suspectTimeSeconds = 0.0f;
    };
}
