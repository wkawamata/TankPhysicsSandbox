#include "TankCollisionProfile.h"

#include <cmath>

namespace Tank::Physics
{
    namespace
    {
        bool IsFinite(const Vec3& value)
        {
            return std::isfinite(value.x) && std::isfinite(value.y) &&
                std::isfinite(value.z);
        }
    }

    bool IsValidTankShape(const TankShapeSettings& shape)
    {
        if (shape.kind != TankShapeKind::Box &&
            shape.kind != TankShapeKind::Capsule)
        {
            return false;
        }
        if (!IsFinite(shape.localPosition) ||
            !IsFinite(shape.localRotationDegrees) || !IsFinite(shape.sizeMeters))
        {
            return false;
        }
        constexpr float minimumSize = 0.05f;
        constexpr float maximumSize = 100.0f;
        if (shape.sizeMeters.x < minimumSize ||
            shape.sizeMeters.y < minimumSize ||
            shape.sizeMeters.z < minimumSize ||
            shape.sizeMeters.x > maximumSize ||
            shape.sizeMeters.y > maximumSize ||
            shape.sizeMeters.z > maximumSize)
        {
            return false;
        }
        if (shape.kind == TankShapeKind::Capsule)
        {
            constexpr float tolerance = 0.0001f;
            if (std::abs(shape.sizeMeters.x - shape.sizeMeters.y) > tolerance ||
                shape.sizeMeters.z + tolerance < shape.sizeMeters.x)
            {
                return false;
            }
        }
        return true;
    }

    bool IsValidTankCollisionProfile(const TankCollisionProfile& profile)
    {
        if (!IsValidTankShape(profile.movementShape) ||
            profile.hitRegions.empty() || profile.hitRegions.size() > 16)
        {
            return false;
        }
        for (const auto& region : profile.hitRegions)
        {
            if (region.name.empty() || region.name.size() > 64 ||
                !IsValidTankShape(region.shape))
            {
                return false;
            }
        }
        return true;
    }
}
