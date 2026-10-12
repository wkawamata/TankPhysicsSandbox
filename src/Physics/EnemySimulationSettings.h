#pragma once
#include <cmath>
namespace Tank::Physics
{
    struct EnemySimulationSettings
    {
        float physicalEnterMeters = 30;
        float simpleReturnMeters = 40;
        float brakingDecelerationMetersPerSecondSquared = 2;
        float nodeArrivalMeters = 0.5f;
        float observationRadiusMeters = 6;
        float supportHeightToleranceMeters = 0.2f;
        float simpleTurnDegreesPerSecond = 60;
    };
    inline bool IsValidEnemySimulationSettings(const EnemySimulationSettings& s)
    {
        return std::isfinite(s.physicalEnterMeters) && std::isfinite(s.simpleReturnMeters) &&
            s.physicalEnterMeters > 0 && s.simpleReturnMeters > s.physicalEnterMeters &&
            std::isfinite(s.brakingDecelerationMetersPerSecondSquared) && s.brakingDecelerationMetersPerSecondSquared > 0 &&
            std::isfinite(s.nodeArrivalMeters) && s.nodeArrivalMeters > 0 &&
            std::isfinite(s.observationRadiusMeters) && s.observationRadiusMeters > 0 &&
            std::isfinite(s.supportHeightToleranceMeters) && s.supportHeightToleranceMeters > 0 &&
            std::isfinite(s.simpleTurnDegreesPerSecond) && s.simpleTurnDegreesPerSecond > 0 && s.simpleTurnDegreesPerSecond <= 360;
    }
}