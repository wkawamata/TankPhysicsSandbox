#include "TankSettingsJson.h"

#include <nlohmann/json.hpp>

namespace Tank::Physics
{
    namespace
    {
        constexpr int kSchemaVersion = 3;

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
        json["endWheelRadiusM"] = settings.endWheelRadiusM;
        json["roadWheelRadiusM"] = settings.roadWheelRadiusM;
        json["roadWheelCount"] = settings.roadWheelCount;
        json["endWheelOffsetM"] = settings.endWheelOffsetM;
        json["twoRoadWheelOffsetM"] = settings.twoRoadWheelOffsetM;
        json["threeRoadWheelOffsetM"] = settings.threeRoadWheelOffsetM;
        json["rideHeightScale"] = settings.rideHeightScale;
        json["neutralBrakeEnabled"] = settings.neutralBrakeEnabled;
        json["neutralBrakeAmount"] = settings.neutralBrakeAmount;
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
        const auto legacyWheelRadius = json.find("wheelRadiusM");
        if (schemaVersion == 1 &&
            legacyWheelRadius != json.end() &&
            legacyWheelRadius->is_number())
        {
            const float radius = legacyWheelRadius->get<float>();
            loaded.endWheelRadiusM = radius;
            loaded.roadWheelRadiusM = radius;
        }
        ReadFloat(json, "endWheelRadiusM", loaded.endWheelRadiusM);
        ReadFloat(json, "roadWheelRadiusM", loaded.roadWheelRadiusM);
        ReadInt(json, "roadWheelCount", loaded.roadWheelCount);
        ReadFloat(json, "endWheelOffsetM", loaded.endWheelOffsetM);
        ReadFloat(json, "twoRoadWheelOffsetM", loaded.twoRoadWheelOffsetM);
        ReadFloat(json, "threeRoadWheelOffsetM", loaded.threeRoadWheelOffsetM);
        ReadFloat(json, "rideHeightScale", loaded.rideHeightScale);
        ReadBool(json, "neutralBrakeEnabled", loaded.neutralBrakeEnabled);
        ReadFloat(json, "neutralBrakeAmount", loaded.neutralBrakeAmount);
        ReadBool(json, "startUpsideDown", loaded.startUpsideDown);
        settings = loaded;
        if (error != nullptr)
        {
            error->clear();
        }
        return true;
    }
}
