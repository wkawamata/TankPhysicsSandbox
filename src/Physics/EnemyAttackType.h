#pragma once

#include "PhysicsTypes.h"
#include <cmath>
#include <string>
#include <vector>

namespace Tank::Physics
{
    enum class EnemyProjectileKind { Ordinary, Special };
    enum class EnemyProjectileShape { Sphere, Box };

    struct EnemyAttackType
    {
        std::string name = "Type 0";
        float detectionRangeMeters = 50.0f;
        float reachMeters = 30.0f;
        float firingRangeMeters = 25.0f;
        float projectileSpeedMetersPerSecond = 1.0f;
        float firingIntervalSeconds = 3.0f;
        float maximumYawSpeedDegreesPerSecond = 5.0f;
        float firingToleranceDegrees = 3.0f;
        EnemyProjectileKind projectileKind = EnemyProjectileKind::Ordinary;
        float projectileRadiusMeters = 0.25f;
        EnemyProjectileShape projectileShape = EnemyProjectileShape::Sphere;
        Vec3 projectileBoxSizeMeters = {1, 1, 1}; // Full dimensions: width, height, depth (+Z flight axis).
    };

    inline bool IsValidEnemyAttackType(const EnemyAttackType& type)
    {
        const auto validSize = [](float value) { return std::isfinite(value) && value >= 0.1f && value <= 20.0f; };
        return (type.projectileShape == EnemyProjectileShape::Sphere || type.projectileShape == EnemyProjectileShape::Box) &&
            (type.projectileKind != EnemyProjectileKind::Ordinary || type.projectileShape == EnemyProjectileShape::Sphere) &&
            validSize(type.projectileBoxSizeMeters.x) && validSize(type.projectileBoxSizeMeters.y) && validSize(type.projectileBoxSizeMeters.z) &&
            (type.projectileKind == EnemyProjectileKind::Ordinary || type.projectileKind == EnemyProjectileKind::Special) &&
            std::isfinite(type.projectileRadiusMeters) && type.projectileRadiusMeters >= 0.05f && type.projectileRadiusMeters <= 10.0f &&
            !type.name.empty() && std::isfinite(type.detectionRangeMeters) &&
            std::isfinite(type.reachMeters) && std::isfinite(type.firingRangeMeters) &&
            std::isfinite(type.projectileSpeedMetersPerSecond) && std::isfinite(type.firingIntervalSeconds) &&
            std::isfinite(type.maximumYawSpeedDegreesPerSecond) && std::isfinite(type.firingToleranceDegrees) &&
            type.detectionRangeMeters >= type.reachMeters && type.reachMeters >= type.firingRangeMeters &&
            type.firingRangeMeters > 0 && type.projectileSpeedMetersPerSecond > 0 &&
            type.firingIntervalSeconds > 0 && type.maximumYawSpeedDegreesPerSecond > 0 &&
            type.firingToleranceDegrees >= 0 && type.firingToleranceDegrees <= 180;
    }

    struct EnemyAttackMount
    {
        // Enemy-local position. Each mount will own independent aiming/fire state.
        Vec3 localPosition = {};
        int attackTypeIndex = 0;
    };

    struct EnemyUnitType
    {
        std::string name = "Fixed turret";
        // Movement data is independent of attack definitions.
        std::vector<EnemyAttackMount> attackMounts = {EnemyAttackMount{}};
    };

    struct EnemyEditorSettings
    {
        std::vector<EnemyAttackType> attackTypes = {EnemyAttackType{}};
        std::vector<EnemyUnitType> unitTypes = {EnemyUnitType{}};
    };
}
