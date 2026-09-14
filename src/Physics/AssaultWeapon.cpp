#include "AssaultWeapon.h"

#include <algorithm>
#include <cmath>

namespace Tank::Physics
{
    AssaultWeapon::AssaultWeapon(const AssaultWeaponSettings& settings)
        : m_settings(settings)
    {
        m_settings.damagePerRound = std::max(0.0f, m_settings.damagePerRound);
        m_settings.roundsPerSecond = std::max(0.0f, m_settings.roundsPerSecond);
    }

    bool AssaultWeapon::TryFire()
    {
        if (m_settings.roundsPerSecond <= 0.0f ||
            m_snapshot.secondsUntilNextRound > 0.0f)
        {
            return false;
        }

        ++m_snapshot.roundsFired;
        m_snapshot.secondsUntilNextRound = 1.0f / m_settings.roundsPerSecond;
        return true;
    }

    void AssaultWeapon::Update(float deltaTimeSeconds)
    {
        if (!std::isfinite(deltaTimeSeconds) || deltaTimeSeconds <= 0.0f)
        {
            return;
        }

        m_snapshot.secondsUntilNextRound = std::max(
            0.0f, m_snapshot.secondsUntilNextRound - deltaTimeSeconds);
        if (m_snapshot.secondsUntilNextRound <= 0.00001f)
        {
            m_snapshot.secondsUntilNextRound = 0.0f;
        }
    }

    AssaultHitResult AssaultWeapon::ApplyHit(CombatTarget& target) const
    {
        AssaultHitResult result;
        if (!target.active)
        {
            return result;
        }

        result.hit = true;
        if (target.kind == CombatTargetKind::EnemyProjectile)
        {
            target.active = false;
            result.destroyed = true;
            result.interceptedProjectile = true;
            return result;
        }

        target.hitPoints -= m_settings.damagePerRound;
        if (target.hitPoints <= 0.0f)
        {
            target.hitPoints = 0.0f;
            target.active = false;
            result.destroyed = true;
        }
        return result;
    }
}
