#include "EnemyNavigationSetup.h"
#include <nlohmann/json.hpp>
#include <cmath>
#include <set>
#include <stdexcept>

namespace Tank::Physics
{
    bool DeserializeEnemyNavigationSetup(const std::string& text, EnemyNavigationSetup& setup, std::string& error)
    {
        try
        {
            const auto json = nlohmann::json::parse(text);
            if (!json.at("version").is_number_integer() || json.at("version") != 1 || !json.at("drivers").is_array())
                throw std::runtime_error("Invalid enemy navigation version or drivers");
            EnemyNavigationSetup candidate;
            if (!DeserializeNavigationGrid(json.at("grid").dump(), candidate.grid, error)) return false;
            std::set<std::string> groups, enemies;
            for (const auto& entry : json.at("drivers"))
            {
                EnemyNavigationDriver driver;
                driver.groupId = entry.at("groupId").get<std::string>();
                driver.enemyId = entry.at("enemyId").get<std::string>();
                driver.surfaceId = entry.at("surfaceId").get<std::string>();
                const auto& snap = entry.at("snapDistanceMeters");
                if (!snap.is_number()) throw std::runtime_error("snapDistanceMeters must be numeric");
                driver.snapDistanceMeters = snap.get<double>();
                if (driver.groupId.empty() || driver.enemyId.empty() || driver.surfaceId.empty() ||
                    !groups.insert(driver.groupId).second || !enemies.insert(driver.enemyId).second ||
                    !std::isfinite(driver.snapDistanceMeters) || driver.snapDistanceMeters <= 0)
                    throw std::runtime_error("Invalid driver or multiple moving drivers in a group");
                if (!DeserializeNavigationMobility(entry.at("mobility").dump(), driver.mobility, error)) return false;
                candidate.drivers.push_back(std::move(driver));
            }
            setup = std::move(candidate);
            error.clear();
            return true;
        }
        catch (const std::exception& exception) { error = exception.what(); return false; }
    }
}
