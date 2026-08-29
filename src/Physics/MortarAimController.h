#pragma once

namespace Tank::Physics
{
    struct MortarAimSettings
    {
        float minimumFireAngleDegrees = 18.0f;
        float maximumAngleDegrees = 65.0f;
        float angleRateDegreesPerSecond = 12.0f;
        float minimumRangeMeters = 8.0f;
        float maximumRangeMeters = 40.0f;
    };

    struct MortarAimSnapshot
    {
        float angleDegrees = 0.0f;
        float rangeMeters = 0.0f;
        bool canFire = false;
        bool atMaximum = false;
    };

    class MortarAimController
    {
    public:
        explicit MortarAimController(const MortarAimSettings& settings = {});
        MortarAimSnapshot Update(float deltaTimeSeconds);
        void Reset();
        const MortarAimSnapshot& Snapshot() const { return m_snapshot; }

    private:
        MortarAimSettings m_settings = {};
        MortarAimSnapshot m_snapshot = {};
    };
}
