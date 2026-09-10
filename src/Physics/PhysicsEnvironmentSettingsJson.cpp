#include "PhysicsEnvironmentSettingsJson.h"

#include <nlohmann/json.hpp>

namespace Tank::Physics
{
    namespace
    {
        constexpr int kSchemaVersion = 1;

        void ReadFloat(
            const nlohmann::json& object,
            const char* name,
            float& value)
        {
            const auto entry = object.find(name);
            if (entry != object.end() && entry->is_number())
            {
                value = entry->get<float>();
            }
        }

        void ReadBool(
            const nlohmann::json& object,
            const char* name,
            bool& value)
        {
            const auto entry = object.find(name);
            if (entry != object.end() && entry->is_boolean())
            {
                value = entry->get<bool>();
            }
        }

        void ReadInt(
            const nlohmann::json& object,
            const char* name,
            int& value)
        {
            const auto entry = object.find(name);
            if (entry != object.end() && entry->is_number_integer())
            {
                value = entry->get<int>();
            }
        }

        nlohmann::json SerializeColor(const ColorRgb& color)
        {
            return { color.r, color.g, color.b };
        }

        void ReadColor(
            const nlohmann::json& object,
            const char* name,
            ColorRgb& color)
        {
            const auto entry = object.find(name);
            if (entry == object.end() || !entry->is_array() || entry->size() != 3 ||
                !(*entry)[0].is_number() || !(*entry)[1].is_number() ||
                !(*entry)[2].is_number())
            {
                return;
            }
            color = {
                (*entry)[0].get<float>(),
                (*entry)[1].get<float>(),
                (*entry)[2].get<float>() };
        }
    }

    std::string SerializePhysicsEnvironmentSettings(
        const PhysicsEnvironmentSettings& settings)
    {
        nlohmann::json json;
        json["version"] = kSchemaVersion;
        json["floorSizeM"] = settings.floorSizeM;
        json["floorFriction"] = settings.floorFriction;
        json["gridEnabled"] = settings.gridEnabled;
        json["gridSpacingM"] = settings.gridSpacingM;
        json["obstacleCount"] = settings.obstacleCount;
        json["obstacleSeed"] = settings.obstacleSeed;
        json["obstacleAreaSizeM"] = settings.obstacleAreaSizeM;
        json["groundColor"] = SerializeColor(settings.groundColor);
        json["gridLineColor"] = SerializeColor(settings.gridLineColor);
        return json.dump(2);
    }

    bool DeserializePhysicsEnvironmentSettings(
        const std::string& jsonText,
        PhysicsEnvironmentSettings& settings,
        std::string* error)
    {
        const nlohmann::json json =
            nlohmann::json::parse(jsonText, nullptr, false);
        if (json.is_discarded() || !json.is_object())
        {
            if (error != nullptr)
            {
                *error = "invalid JSON";
            }
            return false;
        }

        int schemaVersion = 1;
        const auto version = json.find("version");
        if (version != json.end())
        {
            if (!version->is_number_integer())
            {
                if (error != nullptr)
                {
                    *error = "version must be an integer";
                }
                return false;
            }
            schemaVersion = version->get<int>();
        }
        if (schemaVersion < 1 || schemaVersion > kSchemaVersion)
        {
            if (error != nullptr)
            {
                *error = "unsupported version";
            }
            return false;
        }

        PhysicsEnvironmentSettings loaded = settings;
        ReadFloat(json, "floorSizeM", loaded.floorSizeM);
        ReadFloat(json, "floorFriction", loaded.floorFriction);
        ReadBool(json, "gridEnabled", loaded.gridEnabled);
        ReadFloat(json, "gridSpacingM", loaded.gridSpacingM);
        ReadInt(json, "obstacleCount", loaded.obstacleCount);
        ReadInt(json, "obstacleSeed", loaded.obstacleSeed);
        ReadFloat(json, "obstacleAreaSizeM", loaded.obstacleAreaSizeM);
        ReadColor(json, "groundColor", loaded.groundColor);
        ReadColor(json, "gridLineColor", loaded.gridLineColor);
        settings = loaded;
        if (error != nullptr)
        {
            error->clear();
        }
        return true;
    }
}
