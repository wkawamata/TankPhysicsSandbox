#pragma once

#include <cstdint>

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
