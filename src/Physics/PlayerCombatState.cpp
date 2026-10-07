#include "PlayerCombatState.h"

#include <algorithm>
#include <cmath>

namespace Tank::Physics
{
    bool PlayerCombatState::Initialize(const CombatSettings& settings, std::string& error)
    {
        if (!ValidateCombatSettings(settings, error)) return false;
        m_settings = settings;
        m_snapshot = {settings.maximumHitPoints, settings.startingLives, PlayerCombatPhase::Alive, {}};
        return true;
    }

    bool PlayerCombatState::ApplyDamage(float damage, const Vec3& position)
    {
        if (m_snapshot.phase != PlayerCombatPhase::Alive || !std::isfinite(damage) || damage <= 0.0f ||
            !std::isfinite(position.x) || !std::isfinite(position.y) || !std::isfinite(position.z)) return false;
        m_snapshot.hitPoints = (std::max)(0.0f, m_snapshot.hitPoints - damage);
        if (m_snapshot.hitPoints == 0.0f)
        {
            --m_snapshot.lives;
            m_snapshot.lostPosition = position;
            m_snapshot.phase = m_snapshot.lives > 0 ? PlayerCombatPhase::Lost : PlayerCombatPhase::GameOver;
        }
        return true;
    }

    bool PlayerCombatState::ApplyEnemyContact(const Vec3& position)
    {
        return ApplyDamage(m_settings.enemyContactDamage, position);
    }

    bool PlayerCombatState::Respawn()
    {
        if (m_snapshot.phase != PlayerCombatPhase::Lost) return false;
        m_snapshot.hitPoints = m_settings.maximumHitPoints;
        m_snapshot.phase = PlayerCombatPhase::Alive;
        return true;
    }

    bool PlayerCombatState::Continue()
    {
        if (m_snapshot.phase != PlayerCombatPhase::GameOver) return false;
        m_snapshot = {m_settings.maximumHitPoints, m_settings.startingLives, PlayerCombatPhase::Alive, {}};
        return true;
    }
}
