#include "CombatSettings.h"

#include <cmath>
#include <limits>
#include <nlohmann/json.hpp>

namespace Tank::Physics
{
    bool ValidateCombatSettings(const CombatSettings& settings, std::string& error)
    {
        error.clear();
        if (!std::isfinite(settings.maximumHitPoints) || settings.maximumHitPoints <= 0.0f)
            error = "maximumHitPoints must be finite and positive";
        else if (settings.startingLives <= 0)
            error = "startingLives must be positive";
        else if (!std::isfinite(settings.enemyContactDamage) || settings.enemyContactDamage < 0.0f)
            error = "enemyContactDamage must be finite and nonnegative";
        return error.empty();
    }

    bool SerializeCombatSettings(const CombatSettings& settings, std::string& text, std::string& error)
    {
        if (!ValidateCombatSettings(settings, error)) return false;
        text = nlohmann::json{{"version", 1}, {"maximumHitPoints", settings.maximumHitPoints},
            {"startingLives", settings.startingLives}, {"enemyContactDamage", settings.enemyContactDamage}}.dump(2);
        return true;
    }

    bool DeserializeCombatSettings(const std::string& text, CombatSettings& settings, std::string& error)
    {
        error.clear();
        try
        {
            const auto json = nlohmann::json::parse(text);
            if (!json.is_object()) { error = "combat settings must be an object"; return false; }
            if (json.contains("version") && json.at("version") != nlohmann::json(1))
            { error = "unsupported combat settings version"; return false; }
            CombatSettings candidate;
            for (const auto name : {"maximumHitPoints", "enemyContactDamage"})
            {
                if (!json.contains(name)) continue;
                if (!json.at(name).is_number()) { error = std::string(name) + " must be a number"; return false; }
                const float value = json.at(name).get<float>();
                if (std::string(name) == "maximumHitPoints") candidate.maximumHitPoints = value;
                else candidate.enemyContactDamage = value;
            }
            if (json.contains("startingLives"))
            {
                const auto& value = json.at("startingLives");
                if (!value.is_number_integer() || value < 1 || value > (std::numeric_limits<int>::max)())
                { error = "startingLives must be a positive integer in range"; return false; }
                candidate.startingLives = value.get<int>();
            }
            if (!ValidateCombatSettings(candidate, error)) return false;
            settings = candidate;
            return true;
        }
        catch (const nlohmann::json::exception& exception)
        {
            error = exception.what();
            return false;
        }
    }
}
