#pragma once

#include "TankCollisionProfile.h"

#include <cstddef>
#include <optional>
#include <span>

namespace Tank::Physics
{
    struct TransformState;

    struct TankHitRegionTrace
    {
        float fraction = 1.0f;
        size_t regionIndex = 0;
        Vec3 position = {};
        Vec3 normal = {};
    };

    std::optional<TankHitRegionTrace> TraceTankHitRegions(
        const Vec3& start,
        const Vec3& end,
        const TransformState& tankTransform,
        std::span<const TankHitRegion> regions);
}
