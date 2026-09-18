#pragma once

#include <cstdint>
#include "PhysicsTypes.h"

namespace Tank::Physics
{
    // Combat values are deliberately independent of Jolt bodies and rendering.
    // World adapters provide targets and turn fired rounds into visible projectiles.
    enum class CombatTargetKind
    {
        Enemy,
        Destructible,
        EnemyProjectile,
    };

    struct CombatTarget
    {
        std::uint64_t id = 0;
        CombatTargetKind kind = CombatTargetKind::Enemy;
        float hitPoints = 0.0f;
        bool active = false;
    };

    struct AssaultWeaponSettings
    {
        float damagePerRound = 20.0f;
        float roundsPerSecond = 8.0f;
    };

    struct AssaultProjectileSettings
    {
        int maximumCount = 64;
        float speedMetersPerSecond = 80.0f;
        float damagePerRound = 20.0f;
        // Zero means infinite; do not serialize IEEE infinity into JSON.
        float lifetimeSeconds = 0.0f;
        int maximumImpactMarks = 128;
        bool expireAtMaximumDistance = true;
        float maximumDistanceMeters = 40.0f;
    };

    struct AssaultProjectileState
    {
        Vec3 position = {};
        Vec3 velocity = {};
        float damage = 20.0f;
        float ageSeconds = 0.0f;
        float lifetimeSeconds = 0.0f;
        float distanceTraveledMeters = 0.0f;
        bool expireAtMaximumDistance = true;
        float maximumDistanceMeters = 40.0f;
    };

    struct AssaultWeaponSnapshot
    {
        std::uint64_t roundsFired = 0;
        float secondsUntilNextRound = 0.0f;
    };

    struct AssaultHitResult
    {
        bool hit = false;
        bool destroyed = false;
        bool interceptedProjectile = false;
    };

    class AssaultWeapon
    {
    public:
        explicit AssaultWeapon(const AssaultWeaponSettings& settings = {});

        // Returns true once for every permitted round. Calling this every fixed
        // physics step while the trigger is held provides automatic fire.
        bool TryFire();
        void Update(float deltaTimeSeconds);
        AssaultHitResult ApplyHit(CombatTarget& target) const;

        const AssaultWeaponSnapshot& Snapshot() const { return m_snapshot; }

    private:
        AssaultWeaponSettings m_settings = {};
        AssaultWeaponSnapshot m_snapshot = {};
    };
}
