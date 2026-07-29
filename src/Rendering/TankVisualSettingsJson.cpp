#include "TankVisualSettingsJson.h"

#include <nlohmann/json.hpp>

namespace Tank::Rendering
{
    namespace
    {
        constexpr int kSchemaVersion = 1;

        nlohmann::json SerializeMaterial(const BodyMaterialSettings& material)
        {
            return {
                { "albedo", { material.albedo.r, material.albedo.g, material.albedo.b } },
                { "roughness", material.roughness },
                { "metallic", material.metallic },
                { "ambientOcclusion", material.ambientOcclusion },
                { "emissive", material.emissive },
            };
        }

        void ReadFloat(const nlohmann::json& object, const char* name, float& value)
        {
            const auto entry = object.find(name);
            if (entry != object.end() && entry->is_number())
            {
                value = entry->get<float>();
            }
        }

        void ReadMaterial(
            const nlohmann::json& object,
            const char* name,
            BodyMaterialSettings& material)
        {
            const auto entry = object.find(name);
            if (entry == object.end() || !entry->is_object())
            {
                return;
            }

            const auto albedo = entry->find("albedo");
            if (albedo != entry->end() && albedo->is_array() && albedo->size() == 3 &&
                (*albedo)[0].is_number() && (*albedo)[1].is_number() &&
                (*albedo)[2].is_number())
            {
                material.albedo = {
                    (*albedo)[0].get<float>(),
                    (*albedo)[1].get<float>(),
                    (*albedo)[2].get<float>() };
            }
            ReadFloat(*entry, "roughness", material.roughness);
            ReadFloat(*entry, "metallic", material.metallic);
            ReadFloat(*entry, "ambientOcclusion", material.ambientOcclusion);
            ReadFloat(*entry, "emissive", material.emissive);
        }
    }

    std::string SerializeTankVisualSettings(const TankVisualSettings& settings)
    {
        nlohmann::json json;
        json["version"] = kSchemaVersion;
        json["hullUpper"] = SerializeMaterial(settings.hullUpper);
        json["hullLower"] = SerializeMaterial(settings.hullLower);
        json["structureUpper"] = SerializeMaterial(settings.structureUpper);
        json["structureLower"] = SerializeMaterial(settings.structureLower);
        return json.dump(2);
    }

    bool DeserializeTankVisualSettings(
        const std::string& jsonText,
        TankVisualSettings& settings,
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

        const auto version = json.find("version");
        if (version != json.end() &&
            (!version->is_number_integer() || version->get<int>() != kSchemaVersion))
        {
            if (error != nullptr)
            {
                *error = "unsupported version";
            }
            return false;
        }

        TankVisualSettings loaded = settings;
        ReadMaterial(json, "hullUpper", loaded.hullUpper);
        ReadMaterial(json, "hullLower", loaded.hullLower);
        ReadMaterial(json, "structureUpper", loaded.structureUpper);
        ReadMaterial(json, "structureLower", loaded.structureLower);
        settings = loaded;
        if (error != nullptr)
        {
            error->clear();
        }
        return true;
    }
}
