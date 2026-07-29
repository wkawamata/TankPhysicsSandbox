#include "TankSettingsJson.h"

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
    }

    std::string SerializeTankSettings(const TankSettings& settings)
    {
        nlohmann::json json;
        json["version"] = kSchemaVersion;
        json["chassisMassKg"] = settings.chassisMassKg;
        json["rollingInputEnabled"] = settings.rollingInputEnabled;
        json["rollTorqueNm"] = settings.rollTorqueNm;
        json["rollDistanceM"] = settings.rollDistanceM;
        json["rollTorqueCutoffDegrees"] = settings.rollTorqueCutoffDegrees;
        json["rollStabilizationTorqueNm"] = settings.rollStabilizationTorqueNm;
        json["rollStabilizationDampingNms"] = settings.rollStabilizationDampingNms;
        json["trackWidthM"] = settings.trackWidthM;
        json["trackSpacingM"] = settings.trackSpacingM;
        json["chassisWidthM"] = settings.chassisWidthM;
        json["chassisLengthM"] = settings.chassisLengthM;
        json["wheelRadiusM"] = settings.wheelRadiusM;
        json["roadWheelCount"] = settings.roadWheelCount;
        json["endWheelOffsetM"] = settings.endWheelOffsetM;
        json["twoRoadWheelOffsetM"] = settings.twoRoadWheelOffsetM;
        json["threeRoadWheelOffsetM"] = settings.threeRoadWheelOffsetM;
        json["rideHeightScale"] = settings.rideHeightScale;
        json["startUpsideDown"] = settings.startUpsideDown;
        return json.dump(2);
    }

    bool DeserializeTankSettings(
        const std::string& jsonText,
        TankSettings& settings,
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

        TankSettings loaded = settings;
        ReadFloat(json, "chassisMassKg", loaded.chassisMassKg);
        ReadBool(json, "rollingInputEnabled", loaded.rollingInputEnabled);
        ReadFloat(json, "rollTorqueNm", loaded.rollTorqueNm);
        ReadFloat(json, "rollDistanceM", loaded.rollDistanceM);
        ReadFloat(json, "rollTorqueCutoffDegrees", loaded.rollTorqueCutoffDegrees);
        ReadFloat(json, "rollStabilizationTorqueNm", loaded.rollStabilizationTorqueNm);
        ReadFloat(json, "rollStabilizationDampingNms", loaded.rollStabilizationDampingNms);
        ReadFloat(json, "trackWidthM", loaded.trackWidthM);
        ReadFloat(json, "trackSpacingM", loaded.trackSpacingM);
        ReadFloat(json, "chassisWidthM", loaded.chassisWidthM);
        ReadFloat(json, "chassisLengthM", loaded.chassisLengthM);
        ReadFloat(json, "wheelRadiusM", loaded.wheelRadiusM);
        ReadInt(json, "roadWheelCount", loaded.roadWheelCount);
        ReadFloat(json, "endWheelOffsetM", loaded.endWheelOffsetM);
        ReadFloat(json, "twoRoadWheelOffsetM", loaded.twoRoadWheelOffsetM);
        ReadFloat(json, "threeRoadWheelOffsetM", loaded.threeRoadWheelOffsetM);
        ReadFloat(json, "rideHeightScale", loaded.rideHeightScale);
        ReadBool(json, "startUpsideDown", loaded.startUpsideDown);
        settings = loaded;
        if (error != nullptr)
        {
            error->clear();
        }
        return true;
    }
}
