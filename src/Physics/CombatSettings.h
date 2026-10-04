#pragma once

#include <string>

namespace Tank::Physics
{
    struct CombatSettings
    {
        float maximumHitPoints = 100.0f;
        int startingLives = 3;
        float enemyContactDamage = 100.0f;
    };

    bool ValidateCombatSettings(const CombatSettings& settings, std::string& error);
    bool SerializeCombatSettings(const CombatSettings& settings, std::string& text, std::string& error);
    // Missing fields use defaults. Failure leaves settings unchanged.
    bool DeserializeCombatSettings(const std::string& text, CombatSettings& settings, std::string& error);
}
