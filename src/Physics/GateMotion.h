#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <string>

namespace Tank::Physics
{
    struct GateState
    {
        std::string placementId;
        std::array<float, 3> triggerCenter = {};
        std::array<float, 3> travel = {};
        float triggerRadiusMeters = 12;
        float openingSpeedMetersPerSecond = 2;
        bool triggered = false;
        float fraction = 0;

        std::array<float, 3> Offset() const
        { return {travel[0] * fraction, travel[1] * fraction, travel[2] * fraction}; }

        void Advance(const std::array<float, 3>& playerCenter, float dt)
        {
            if (!std::isfinite(dt) || dt <= 0) return;
            double distanceSquared = 0, lengthSquared = 0;
            for (size_t axis = 0; axis < 3; ++axis)
            {
                const double delta = double(playerCenter[axis]) - triggerCenter[axis];
                distanceSquared += delta * delta;
                lengthSquared += double(travel[axis]) * travel[axis];
            }
            // Proximity is the initial event source; opening remains latched until reset.
            triggered |= distanceSquared <= double(triggerRadiusMeters) * triggerRadiusMeters;
            if (triggered && lengthSquared > 0)
                fraction = std::min(1.0f, fraction + float(openingSpeedMetersPerSecond * dt / std::sqrt(lengthSquared)));
        }
    };
}
