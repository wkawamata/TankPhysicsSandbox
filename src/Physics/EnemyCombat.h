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
    };
    inline float WrapEnemyAngle(float angle) { return std::atan2(std::sin(angle), std::cos(angle)); }
    inline bool UpdateEnemyAim(EnemyAimState& state, const EnemyAttackType& type,
        const Vec3& origin, const Vec3& player, float dt, bool visible, bool active)
    {
        if (!std::isfinite(dt) || dt <= 0 || !IsValidEnemyAttackType(type)) return false;
        const float x = player.x - origin.x, z = player.z - origin.z;
        const float distance = std::hypot(x, z);
        state.detected = active && visible && distance <= type.detectionRangeMeters;
        state.withinReach = state.detected && distance <= type.reachMeters;
        state.canFire = false;
        if (!state.detected) { state.secondsUntilNextShot = type.firingIntervalSeconds; return false; }
        const float desired = std::atan2(x, z);
        constexpr float radians = 3.14159265358979323846f / 180;
        const float maximum = type.maximumYawSpeedDegreesPerSecond * radians * dt;
        state.yawRadians = WrapEnemyAngle(state.yawRadians + std::clamp(WrapEnemyAngle(desired - state.yawRadians), -maximum, maximum));
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
