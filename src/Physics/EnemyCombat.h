#pragma once
#include "EnemyAttackType.h"
#include "AssaultWeapon.h"
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace Tank::Physics
{
    inline constexpr size_t kMaximumEnemyProjectiles = 256;
    struct EnemyAimState
    {
        float yawRadians = 0;
        float secondsUntilNextShot = 0;
        bool detected = false;
        bool withinReach = false;
        bool canFire = false;
        std::uint64_t shotsFired = 0;
        float initialYawRadians = 0;
        bool alert = false;
        float alertSecondsRemaining = 0;
        Vec3 lastKnownPosition;
    };
    struct EnemyMountState
    {
        EnemyAttackType attackType;
        Vec3 localPosition;
        Vec3 worldPosition;
        EnemyAimState aim;
    };
    struct EnemyProjectileState
    {
        CombatTarget target;
        std::uint64_t ownerId = 0;
        Vec3 position;
        Vec3 velocity;
        float remainingDistance = 30;
        float radius = 0.25f;
        float damage = 100;
        EnemyProjectileShape shape = EnemyProjectileShape::Sphere;
        Vec3 boxSizeMeters = {1, 1, 1};
        Quat rotation;

    };
    // Rotate the local +Z flight axis toward the shot's direction. Fixed after launch.
    inline Quat EnemyProjectileRotation(const Vec3& velocity)
    {
        const float length = std::hypot(velocity.x, velocity.y, velocity.z);
        if (length < 0.000001f) return {};
        const float x=velocity.x/length, y=velocity.y/length, z=velocity.z/length;
        if (z < -0.999999f) return {0, 1, 0, 0};
        const float w=1+z;
        const float scale=1/std::sqrt(x*x+y*y+w*w);
        return {-y*scale, x*scale, 0, w*scale};
    }
    inline float WrapEnemyAngle(float angle) { return std::atan2(std::sin(angle), std::cos(angle)); }
    inline bool UpdateEnemyAim(EnemyAimState& state, const EnemyAttackType& type,
        const Vec3& origin, const Vec3& player, float dt, bool visible, bool active)
    {
        if (!std::isfinite(dt) || dt <= 0 || !IsValidEnemyAttackType(type)) return false;
        const float x = player.x - origin.x, z = player.z - origin.z;
        const float distance = std::hypot(x, z);
        state.detected = active && (visible || type.detectionMode == EnemyDetectionMode::RangeOnly) &&
            distance <= type.detectionRangeMeters;
        state.withinReach = state.detected && distance <= type.reachMeters;
        state.canFire = false;
        if (state.detected)
        {
            state.alert = true;
            state.alertSecondsRemaining = type.alertReleaseSeconds;
            state.lastKnownPosition = player;
        }
        else
        {
            state.alertSecondsRemaining = active ? std::max(0.0f, state.alertSecondsRemaining - dt) : 0;
            state.alert = active && state.alert && state.alertSecondsRemaining > 0;
            state.secondsUntilNextShot = type.firingIntervalSeconds;
        }
        const float desired = state.alert ? std::atan2(state.lastKnownPosition.x - origin.x,
            state.lastKnownPosition.z - origin.z) : state.initialYawRadians;
        constexpr float radians = 3.14159265358979323846f / 180;
        const float maximum = type.maximumYawSpeedDegreesPerSecond * radians * dt;
        state.yawRadians = WrapEnemyAngle(state.yawRadians + std::clamp(WrapEnemyAngle(desired - state.yawRadians), -maximum, maximum));
        if (!state.detected) return false;
        state.secondsUntilNextShot = std::max(0.0f, state.secondsUntilNextShot - dt);
        state.canFire = state.withinReach && distance <= type.firingRangeMeters &&
            std::abs(WrapEnemyAngle(desired - state.yawRadians)) <= type.firingToleranceDegrees * radians + 1.0e-6f;
        if (!state.canFire || state.secondsUntilNextShot > 1.0e-5f) return false;
        state.secondsUntilNextShot = type.firingIntervalSeconds;
        ++state.shotsFired;
        return true;
    }
    // Relative swept point/sphere test; returns >1 for no interception this step.
    inline float EnemyInterceptionFraction(const Vec3& start, const Vec3& end,
        const EnemyProjectileState& enemy, float dt)
    {
        const Vec3 p = {start.x - enemy.position.x, start.y - enemy.position.y, start.z - enemy.position.z};
        const Vec3 d = {end.x - start.x - enemy.velocity.x * dt,
            end.y - start.y - enemy.velocity.y * dt, end.z - start.z - enemy.velocity.z * dt};
        const float a = d.x*d.x + d.y*d.y + d.z*d.z;
        const float b = p.x*d.x + p.y*d.y + p.z*d.z;
        const float c = p.x*p.x + p.y*p.y + p.z*p.z - enemy.radius*enemy.radius;
        if (c <= 0) return 0;
        const float disc = b*b - a*c;
        if (a <= 1.0e-12f || disc < 0) return 2;
        const float t = (-b - std::sqrt(disc)) / a;
        return t >= 0 && t <= 1 ? t : 2;
    }
}
