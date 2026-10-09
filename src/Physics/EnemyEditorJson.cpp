#include "EnemyEditorJson.h"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <set>

namespace Tank::Physics
{
    namespace
    {
        void Validate(const EnemyEditorSettings& settings)
        {
            if (settings.attackTypes.empty()) throw std::runtime_error("At least one attack type is required");
            std::set<std::string> names;
            for (const auto& type : settings.attackTypes)
                if (!IsValidEnemyAttackType(type) || !names.insert(type.name).second)
                    throw std::runtime_error("Invalid or duplicate attack type name/parameters");
            names.clear();
            for (const auto& unit : settings.unitTypes)
            {
                if (unit.name.empty() || !names.insert(unit.name).second) throw std::runtime_error("Invalid or duplicate enemy name");
                for (const auto& mount : unit.attackMounts)
                    if (mount.attackTypeIndex < 0 || static_cast<size_t>(mount.attackTypeIndex) >= settings.attackTypes.size() ||
                        !std::isfinite(mount.localPosition.x) || !std::isfinite(mount.localPosition.y) || !std::isfinite(mount.localPosition.z))
                        throw std::runtime_error("Invalid attack mount");
            }
        }
    }

    bool SerializeEnemyEditor(const EnemyEditorSettings& settings, std::string& text, std::string& error)
    {
        error.clear();
        try
        {
            Validate(settings);
            nlohmann::json root = {{"version", 1}, {"attackTypes", nlohmann::json::array()}, {"unitTypes", nlohmann::json::array()}};
            for (const auto& type : settings.attackTypes)
                root["attackTypes"].push_back({{"name", type.name}, {"detectionRangeMeters", type.detectionRangeMeters},
                    {"reachMeters", type.reachMeters}, {"firingRangeMeters", type.firingRangeMeters},
                    {"projectileSpeedMetersPerSecond", type.projectileSpeedMetersPerSecond},
                    {"firingIntervalSeconds", type.firingIntervalSeconds},
                    {"maximumYawSpeedDegreesPerSecond", type.maximumYawSpeedDegreesPerSecond},
                    {"firingToleranceDegrees", type.firingToleranceDegrees},
                    {"projectileKind", type.projectileKind == EnemyProjectileKind::Special ? "special" : "ordinary"},
                    {"projectileRadiusMeters", type.projectileRadiusMeters},
                    {"projectileShape", type.projectileShape == EnemyProjectileShape::Box ? "box" : "sphere"},
                    {"projectileBoxSizeMeters", {type.projectileBoxSizeMeters.x, type.projectileBoxSizeMeters.y, type.projectileBoxSizeMeters.z}}});
            for (const auto& unit : settings.unitTypes)
            {
                nlohmann::json entry = {{"name", unit.name}, {"attackMounts", nlohmann::json::array()}};
                for (const auto& mount : unit.attackMounts)
                    entry["attackMounts"].push_back({{"attackType", settings.attackTypes[static_cast<size_t>(mount.attackTypeIndex)].name},
                        {"localPosition", {mount.localPosition.x, mount.localPosition.y, mount.localPosition.z}}});
                root["unitTypes"].push_back(entry);
            }
            text = root.dump(2);
            return true;
        }
        catch (const std::exception& exception) { error = exception.what(); return false; }
    }

    bool DeserializeEnemyEditor(const std::string& text, EnemyEditorSettings& settings, std::string& error)
    {
        error.clear();
        try
        {
            const auto root = nlohmann::json::parse(text);
            if (!root.at("version").is_number_integer() || root.at("version") != 1) throw std::runtime_error("Unsupported version");
            if (!root.at("attackTypes").is_array() || !root.at("unitTypes").is_array()) throw std::runtime_error("Expected type arrays");
            EnemyEditorSettings candidate;
            candidate.attackTypes.clear();
            candidate.unitTypes.clear();
            for (const auto& entry : root.at("attackTypes"))
            {
                EnemyAttackType type;
                type.name = entry.at("name").get<std::string>();
                auto read = [&](const char* key, float& value)
                {
                    if (!entry.contains(key)) return;
                    if (!entry.at(key).is_number()) throw std::runtime_error(std::string(key) + " must be numeric");
                    value = entry.at(key).get<float>();
                };
                read("detectionRangeMeters", type.detectionRangeMeters);
                read("reachMeters", type.reachMeters);
                read("firingRangeMeters", type.firingRangeMeters);
                read("projectileSpeedMetersPerSecond", type.projectileSpeedMetersPerSecond);
                read("firingIntervalSeconds", type.firingIntervalSeconds);
                read("maximumYawSpeedDegreesPerSecond", type.maximumYawSpeedDegreesPerSecond);
                read("firingToleranceDegrees", type.firingToleranceDegrees);
                read("projectileRadiusMeters", type.projectileRadiusMeters);
                if (entry.contains("projectileKind"))
                {
                    const auto kind = entry.at("projectileKind").get<std::string>();
                    if (kind == "special") type.projectileKind = EnemyProjectileKind::Special;
                    else if (kind != "ordinary") throw std::runtime_error("Unknown projectile kind: " + kind);
                }
                if (entry.contains("projectileShape"))
                {
                    const auto shape = entry.at("projectileShape").get<std::string>();
                    if (shape == "box") type.projectileShape = EnemyProjectileShape::Box;
                    else if (shape != "sphere") throw std::runtime_error("Unknown projectile shape: " + shape);
                }
                if (entry.contains("projectileBoxSizeMeters"))
                {
                    const auto& size = entry.at("projectileBoxSizeMeters");
                    if (!size.is_array() || size.size()!=3) throw std::runtime_error("Expected three box dimensions");
                    for (const auto& dimension : size) if (!dimension.is_number()) throw std::runtime_error("Box dimensions must be numeric");
                    type.projectileBoxSizeMeters = {size[0].get<float>(), size[1].get<float>(), size[2].get<float>()};
                }
                candidate.attackTypes.push_back(type);
            }
            for (const auto& entry : root.at("unitTypes"))
            {
                EnemyUnitType unit;
                unit.name = entry.at("name").get<std::string>();
                unit.attackMounts.clear();
                if (!entry.at("attackMounts").is_array()) throw std::runtime_error("Expected attackMounts array");
                for (const auto& mountEntry : entry.at("attackMounts"))
                {
                    const auto name = mountEntry.at("attackType").get<std::string>();
                    const auto found = std::find_if(candidate.attackTypes.begin(), candidate.attackTypes.end(),
                        [&](const auto& type) { return type.name == name; });
                    if (found == candidate.attackTypes.end()) throw std::runtime_error("Unknown attack type: " + name);
                    const auto& position = mountEntry.at("localPosition");
                    if (!position.is_array() || position.size() != 3) throw std::runtime_error("Expected three position coordinates");
                    for (const auto& coordinate : position) if (!coordinate.is_number()) throw std::runtime_error("Position must be numeric");
                    unit.attackMounts.push_back({{position[0].get<float>(), position[1].get<float>(), position[2].get<float>()},
                        static_cast<int>(found - candidate.attackTypes.begin())});
                }
                candidate.unitTypes.push_back(unit);
            }
            Validate(candidate);
            settings = std::move(candidate);
            return true;
        }
        catch (const std::exception& exception) { error = exception.what(); return false; }
    }
}
