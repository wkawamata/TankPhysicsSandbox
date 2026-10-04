#pragma once

#include "CombatSettings.h"
#include "PhysicsTypes.h"

namespace Tank::Physics
{
    enum class PlayerCombatPhase { Alive, Lost, GameOver };

    struct PlayerCombatSnapshot
    {
        float hitPoints = 100.0f;
        int lives = 3;
        PlayerCombatPhase phase = PlayerCombatPhase::Alive;
        Vec3 lostPosition = {};
    };

    // World adapters decide when to respawn and move the Tank. This class does
    // not choose respawn delay, invulnerability, or enemy reset policies.
    class PlayerCombatState
    {
    public:
        bool Initialize(const CombatSettings& settings, std::string& error);
        // Returns true only for accepted damage. Lost/GameOver reject all hits.
        bool ApplyDamage(float damage, const Vec3& position);
        bool ApplyEnemyContact(const Vec3& position);
        bool Respawn();
        bool Continue();
        const PlayerCombatSnapshot& Snapshot() const { return m_snapshot; }
        const CombatSettings& Settings() const { return m_settings; }

    private:
        CombatSettings m_settings = {};
        PlayerCombatSnapshot m_snapshot = {};
    };
}
