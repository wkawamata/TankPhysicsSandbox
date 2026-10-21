#pragma once

#include "PhysicsTypes.h"

#include <string>
#include <vector>

namespace Tank::Physics
{
    enum class TankShapeKind
    {
        Box,
        Capsule
    };

    // Tank-local shape. Size contains full dimensions. Capsules use a circular
    // X/Y diameter and run along local +Z; size.z is the total end-to-end length.
    struct TankShapeSettings
    {
        TankShapeKind kind = TankShapeKind::Box;
        Vec3 localPosition = {};
        Vec3 localRotationDegrees = {};
        Vec3 sizeMeters = {1.0f, 1.0f, 1.0f};
        bool operator==(const TankShapeSettings&) const = default;
    };

    struct TankHitRegion
    {
        std::string name = "Hull";
        TankShapeSettings shape = {};
        bool operator==(const TankHitRegion&) const = default;
    };

    // Movement collision and projectile hit testing deliberately use separate
    // shapes. This keeps navigation tuning independent from visible hit effects.
    struct TankCollisionProfile
    {
        TankShapeSettings movementShape = {};
        std::vector<TankHitRegion> hitRegions = {TankHitRegion{}};
        bool operator==(const TankCollisionProfile&) const = default;
    };

    inline TankCollisionProfile MakeTankCollisionProfile(const Vec3& fullSize)
    {
        TankCollisionProfile profile;
        profile.movementShape.sizeMeters = fullSize;
        profile.hitRegions[0].shape.sizeMeters = fullSize;
        return profile;
    }

    bool IsValidTankShape(const TankShapeSettings& shape);
    bool IsValidTankCollisionProfile(const TankCollisionProfile& profile);
}
