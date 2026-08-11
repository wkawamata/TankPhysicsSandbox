#include "CameraSettings.h"

#include <nlohmann/json.hpp>

namespace Tank::Rendering
{
    namespace
    {
        constexpr int kSchemaVersion = 1;

        void ReadFloat(const nlohmann::json& object, const char* name, float& value)
        {
            const auto entry = object.find(name);
            if (entry != object.end() && entry->is_number())
            {
                value = entry->get<float>();
            }
        }

        void ReadVector(
            const nlohmann::json& object,
            const char* name,
            float (&value)[3])
        {
            const auto entry = object.find(name);
            if (entry != object.end() && entry->is_array() && entry->size() == 3 &&
                (*entry)[0].is_number() && (*entry)[1].is_number() &&
                (*entry)[2].is_number())
            {
                value[0] = (*entry)[0].get<float>();
                value[1] = (*entry)[1].get<float>();
                value[2] = (*entry)[2].get<float>();
            }
        }
    }

    std::string SerializeCameraSettings(const CameraSettings& settings)
    {
        return nlohmann::json {
            { "version", kSchemaVersion },
            { "position", settings.position },
            { "gazePoint", settings.gazePoint },
            { "up", settings.up },
            { "projection", settings.projection },
            { "fovDegrees", settings.fovDegrees },
            { "orthographicHeight", settings.orthographicHeight },
            { "followTank", settings.followTank },
            { "followDistance", settings.followDistance },
            { "lookDownDegrees", settings.lookDownDegrees },
            { "followYawOffsetDegrees", settings.followYawOffsetDegrees },
            { "positionSpeed", settings.positionSpeed },
            { "rotationSpeed", settings.rotationSpeed },
            { "damping", settings.damping },
            { "yawSpeedLimitDegrees", settings.yawSpeedLimitDegrees },
            { "yawDamping", settings.yawDamping },
        }.dump(2);
    }

    bool DeserializeCameraSettings(
        const std::string& jsonText,
        CameraSettings& settings,
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
        if (version == json.end() || !version->is_number_integer() ||
            version->get<int>() != kSchemaVersion)
        {
            if (error != nullptr)
            {
                *error = "unsupported version";
            }
            return false;
        }

        CameraSettings loaded = settings;
        ReadVector(json, "position", loaded.position);
        ReadVector(json, "gazePoint", loaded.gazePoint);
        ReadVector(json, "up", loaded.up);
        const auto projection = json.find("projection");
        if (projection != json.end() && projection->is_number_integer())
        {
            loaded.projection = projection->get<int>();
        }
        ReadFloat(json, "fovDegrees", loaded.fovDegrees);
        ReadFloat(json, "orthographicHeight", loaded.orthographicHeight);
        const auto followTank = json.find("followTank");
        if (followTank != json.end() && followTank->is_boolean())
        {
            loaded.followTank = followTank->get<bool>();
        }
        ReadFloat(json, "followDistance", loaded.followDistance);
        ReadFloat(json, "lookDownDegrees", loaded.lookDownDegrees);
        ReadFloat(json, "followYawOffsetDegrees", loaded.followYawOffsetDegrees);
        ReadFloat(json, "positionSpeed", loaded.positionSpeed);
        ReadFloat(json, "rotationSpeed", loaded.rotationSpeed);
        ReadFloat(json, "damping", loaded.damping);
        ReadFloat(json, "yawSpeedLimitDegrees", loaded.yawSpeedLimitDegrees);
        ReadFloat(json, "yawDamping", loaded.yawDamping);
        settings = loaded;
        if (error != nullptr)
        {
            error->clear();
        }
        return true;
    }
}
