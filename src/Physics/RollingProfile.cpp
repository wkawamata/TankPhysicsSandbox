#include "RollingProfile.h"

#include <nlohmann/json.hpp>

namespace Tank::Physics
{
    namespace
    {
        constexpr int kSchemaVersion = 1;

        void ReadFloat(const nlohmann::json& json, const char* name, float& value)
        {
            const auto entry = json.find(name);
            if (entry != json.end() && entry->is_number())
            {
                value = entry->get<float>();
            }
        }

        void ReadBool(const nlohmann::json& json, const char* name, bool& value)
        {
            const auto entry = json.find(name);
            if (entry != json.end() && entry->is_boolean())
            {
                value = entry->get<bool>();
            }
        }
    }

    RollingProfile ExtractRollingProfile(const TankSettings& settings)
    {
        return {
            settings.rollingInputEnabled,
            settings.rollSpeedMultiplier,
            settings.rollTorqueNm,
            settings.rollReturnDecisionDegrees,
            settings.rollApproachStartDegrees,
            settings.rollApproachDampingNms,
            settings.rollCommitTorqueNm,
            settings.rollAirBrakeTorqueNm,
            settings.rollAirBrakeReleaseDegrees,
            settings.rollDistanceMatchesVehicleWidth,
            settings.rollTravelVehicleWidths,
            settings.rollDistanceM,
            settings.rollTorqueCutoffDegrees,
            settings.rollStabilizationTorqueNm,
            settings.rollStabilizationDampingNms,
        };
    }

    void ApplyRollingProfile(const RollingProfile& profile, TankSettings& settings)
    {
        settings.rollingInputEnabled = profile.inputEnabled;
        settings.rollSpeedMultiplier = profile.speedMultiplier;
        settings.rollTorqueNm = profile.torqueNm;
        settings.rollReturnDecisionDegrees = profile.returnDecisionDegrees;
        settings.rollApproachStartDegrees = profile.approachStartDegrees;
        settings.rollApproachDampingNms = profile.approachDampingNms;
        settings.rollCommitTorqueNm = profile.commitTorqueNm;
        settings.rollAirBrakeTorqueNm = profile.airBrakeTorqueNm;
        settings.rollAirBrakeReleaseDegrees = profile.airBrakeReleaseDegrees;
        settings.rollDistanceMatchesVehicleWidth = profile.distanceMatchesVehicleWidth;
        settings.rollTravelVehicleWidths = profile.travelVehicleWidths;
        settings.rollDistanceM = profile.distanceM;
        settings.rollTorqueCutoffDegrees = profile.torqueCutoffDegrees;
        settings.rollStabilizationTorqueNm = profile.stabilizationTorqueNm;
        settings.rollStabilizationDampingNms = profile.stabilizationDampingNms;
    }

    std::string SerializeRollingProfile(const RollingProfile& profile)
    {
        nlohmann::json json;
        json["version"] = kSchemaVersion;
        json["inputEnabled"] = profile.inputEnabled;
        json["speedMultiplier"] = profile.speedMultiplier;
        json["torqueNm"] = profile.torqueNm;
        json["returnDecisionDegrees"] = profile.returnDecisionDegrees;
        json["approachStartDegrees"] = profile.approachStartDegrees;
        json["approachDampingNms"] = profile.approachDampingNms;
        json["commitTorqueNm"] = profile.commitTorqueNm;
        json["airBrakeTorqueNm"] = profile.airBrakeTorqueNm;
        json["airBrakeReleaseDegrees"] = profile.airBrakeReleaseDegrees;
        json["distanceMatchesVehicleWidth"] = profile.distanceMatchesVehicleWidth;
        json["travelVehicleWidths"] = profile.travelVehicleWidths;
        json["distanceM"] = profile.distanceM;
        json["torqueCutoffDegrees"] = profile.torqueCutoffDegrees;
        json["stabilizationTorqueNm"] = profile.stabilizationTorqueNm;
        json["stabilizationDampingNms"] = profile.stabilizationDampingNms;
        return json.dump(2);
    }

    bool DeserializeRollingProfile(
        const std::string& jsonText,
        RollingProfile& profile,
        std::string* error)
    {
        const nlohmann::json json = nlohmann::json::parse(jsonText, nullptr, false);
        if (json.is_discarded() || !json.is_object())
        {
            if (error != nullptr) *error = "invalid JSON";
            return false;
        }

        const auto version = json.find("version");
        if (version != json.end() && (!version->is_number_integer() || version->get<int>() != kSchemaVersion))
        {
            if (error != nullptr) *error = "unsupported version";
            return false;
        }

        RollingProfile loaded = profile;
        ReadBool(json, "inputEnabled", loaded.inputEnabled);
        ReadFloat(json, "speedMultiplier", loaded.speedMultiplier);
        ReadFloat(json, "torqueNm", loaded.torqueNm);
        ReadFloat(json, "returnDecisionDegrees", loaded.returnDecisionDegrees);
        ReadFloat(json, "approachStartDegrees", loaded.approachStartDegrees);
        ReadFloat(json, "approachDampingNms", loaded.approachDampingNms);
        ReadFloat(json, "commitTorqueNm", loaded.commitTorqueNm);
        ReadFloat(json, "airBrakeTorqueNm", loaded.airBrakeTorqueNm);
        ReadFloat(json, "airBrakeReleaseDegrees", loaded.airBrakeReleaseDegrees);
        ReadBool(json, "distanceMatchesVehicleWidth", loaded.distanceMatchesVehicleWidth);
        ReadFloat(json, "travelVehicleWidths", loaded.travelVehicleWidths);
        ReadFloat(json, "distanceM", loaded.distanceM);
        ReadFloat(json, "torqueCutoffDegrees", loaded.torqueCutoffDegrees);
        ReadFloat(json, "stabilizationTorqueNm", loaded.stabilizationTorqueNm);
        ReadFloat(json, "stabilizationDampingNms", loaded.stabilizationDampingNms);
        profile = loaded;
        if (error != nullptr) error->clear();
        return true;
    }
}
