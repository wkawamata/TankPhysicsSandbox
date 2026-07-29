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

        PhysicsEnvironmentSettings loaded = settings;
        ReadFloat(json, "floorSizeM", loaded.floorSizeM);
        ReadFloat(json, "floorFriction", loaded.floorFriction);
        ReadBool(json, "gridEnabled", loaded.gridEnabled);
        ReadFloat(json, "gridSpacingM", loaded.gridSpacingM);
        settings = loaded;
        if (error != nullptr)
        {
            error->clear();
        }
        return true;
    }
}
