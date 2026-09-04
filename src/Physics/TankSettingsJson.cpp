#include "TankSettingsJson.h"

#include <nlohmann/json.hpp>

namespace Tank::Physics
{
    namespace
    {
        constexpr int kSchemaVersion = 17;

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
        json["recoilImpulseNewtonSeconds"] = settings.recoilImpulseNewtonSeconds;
        json["recoilPointForwardM"] = settings.recoilPointForwardM;
        json["recoilPointHeightM"] = settings.recoilPointHeightM;
        json["rollingInputEnabled"] = settings.rollingInputEnabled;
        json["rollTorqueNm"] = settings.rollTorqueNm;
        json["rollAirBrakeTorqueNm"] = settings.rollAirBrakeTorqueNm;
        json["rollDistanceM"] = settings.rollDistanceM;
        json["rollTorqueCutoffDegrees"] = settings.rollTorqueCutoffDegrees;
        json["rollStabilizationTorqueNm"] = settings.rollStabilizationTorqueNm;
        json["rollStabilizationDampingNms"] = settings.rollStabilizationDampingNms;
        json["trackWidthM"] = settings.trackWidthM;
        json["trackSpacingM"] = settings.trackSpacingM;
        json["trackLongitudinalFriction"] = settings.trackLongitudinalFriction;
        json["trackLateralFriction"] = settings.trackLateralFriction;
        json["chassisWidthM"] = settings.chassisWidthM;
        json["chassisLengthM"] = settings.chassisLengthM;
        json["endWheelRadiusM"] = settings.endWheelRadiusM;
        json["roadWheelRadiusM"] = settings.roadWheelRadiusM;
        json["roadWheelCount"] = settings.roadWheelCount;
        json["endWheelOffsetM"] = settings.endWheelOffsetM;
        json["endWheelVerticalOffsetM"] = settings.endWheelVerticalOffsetM;
        json["roadWheelVerticalOffsetM"] = settings.roadWheelVerticalOffsetM;
        json["wheelHorizontalOffsetM"] = settings.wheelHorizontalOffsetM;
        json["twoRoadWheelOffsetM"] = settings.twoRoadWheelOffsetM;
        json["threeRoadWheelOffsetM"] = settings.threeRoadWheelOffsetM;
        json["rideHeightScale"] = settings.rideHeightScale;
        json["suspensionFrequencyHz"] = settings.suspensionFrequencyHz;
        json["suspensionDamping"] = settings.suspensionDamping;
        json["suspensionStrokeMeters"] = settings.suspensionStrokeMeters;
        json["neutralBrakeEnabled"] = settings.neutralBrakeEnabled;
        json["neutralBrakeAmount"] = settings.neutralBrakeAmount;
        json["stationaryTurnInnerTrackRatio"] = settings.stationaryTurnInnerTrackRatio;
        json["stationaryTurnLeftTraction"] = settings.stationaryTurnLeftTraction;
        json["stationaryTurnRightTraction"] = settings.stationaryTurnRightTraction;
        json["pivotTurnLeftTraction"] = settings.pivotTurnLeftTraction;
        json["pivotTurnRightTraction"] = settings.pivotTurnRightTraction;
        json["engineMaxTorqueNm"] = settings.engineMaxTorqueNm;
        json["engineMaxRpm"] = settings.engineMaxRpm;
        json["transmissionShiftDownRpm"] = settings.transmissionShiftDownRpm;
        json["transmissionShiftUpRpm"] = settings.transmissionShiftUpRpm;
        json["transmissionClutchStrength"] = settings.transmissionClutchStrength;
        json["finalDriveRatio"] = settings.finalDriveRatio;
        json["clutchReleaseTimeSeconds"] = settings.clutchReleaseTimeSeconds;
        json["yawSpeedLimitDegrees"] = settings.yawSpeedLimitDegrees;
        json["yawDamping"] = settings.yawDamping;
        json["startUpsideDown"] = settings.startUpsideDown;
        json["stoppedEnterLinearSpeedMetersPerSecond"] = settings.stoppedEnterLinearSpeedMetersPerSecond;
        json["stoppedExitLinearSpeedMetersPerSecond"] = settings.stoppedExitLinearSpeedMetersPerSecond;
        json["stoppedEnterAngularSpeedRadiansPerSecond"] = settings.stoppedEnterAngularSpeedRadiansPerSecond;
        json["stoppedExitAngularSpeedRadiansPerSecond"] = settings.stoppedExitAngularSpeedRadiansPerSecond;
        json["stoppedEnterTrackSlipMetersPerSecond"] = settings.stoppedEnterTrackSlipMetersPerSecond;
        json["stoppedExitTrackSlipMetersPerSecond"] = settings.stoppedExitTrackSlipMetersPerSecond;
        json["stoppedEnterSuspensionSpeedMetersPerSecond"] = settings.stoppedEnterSuspensionSpeedMetersPerSecond;
        json["stoppedExitSuspensionSpeedMetersPerSecond"] = settings.stoppedExitSuspensionSpeedMetersPerSecond;
        json["stoppedMinimumUpAlignment"] = settings.stoppedMinimumUpAlignment;
        json["stoppedConfirmSeconds"] = settings.stoppedConfirmSeconds;
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
        ReadFloat(json, "recoilImpulseNewtonSeconds", loaded.recoilImpulseNewtonSeconds);
        ReadFloat(json, "recoilPointForwardM", loaded.recoilPointForwardM);
        ReadFloat(json, "recoilPointHeightM", loaded.recoilPointHeightM);
        ReadBool(json, "rollingInputEnabled", loaded.rollingInputEnabled);
        ReadFloat(json, "rollTorqueNm", loaded.rollTorqueNm);
        ReadFloat(json, "rollAirBrakeTorqueNm", loaded.rollAirBrakeTorqueNm);
        ReadFloat(json, "rollDistanceM", loaded.rollDistanceM);
        ReadFloat(json, "rollTorqueCutoffDegrees", loaded.rollTorqueCutoffDegrees);
        ReadFloat(json, "rollStabilizationTorqueNm", loaded.rollStabilizationTorqueNm);
        ReadFloat(json, "rollStabilizationDampingNms", loaded.rollStabilizationDampingNms);
        ReadFloat(json, "trackWidthM", loaded.trackWidthM);
        ReadFloat(json, "trackSpacingM", loaded.trackSpacingM);
        ReadFloat(
            json,
            "trackLongitudinalFriction",
            loaded.trackLongitudinalFriction);
        ReadFloat(json, "trackLateralFriction", loaded.trackLateralFriction);
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
        ReadFloat(json, "endWheelVerticalOffsetM", loaded.endWheelVerticalOffsetM);
        ReadFloat(json, "roadWheelVerticalOffsetM", loaded.roadWheelVerticalOffsetM);
        ReadFloat(json, "wheelHorizontalOffsetM", loaded.wheelHorizontalOffsetM);
        ReadFloat(json, "twoRoadWheelOffsetM", loaded.twoRoadWheelOffsetM);
        ReadFloat(json, "threeRoadWheelOffsetM", loaded.threeRoadWheelOffsetM);
        ReadFloat(json, "rideHeightScale", loaded.rideHeightScale);
        ReadFloat(json, "suspensionFrequencyHz", loaded.suspensionFrequencyHz);
        ReadFloat(json, "suspensionDamping", loaded.suspensionDamping);
        const auto suspensionStrokes = json.find("suspensionStrokeMeters");
        if (suspensionStrokes == json.end())
        {
            loaded.suspensionStrokeMeters =
                MakeDefaultSuspensionStrokes(loaded.rideHeightScale);
        }
        else
        {
            if (!suspensionStrokes->is_array() ||
                suspensionStrokes->size() != loaded.suspensionStrokeMeters.size())
            {
                if (error != nullptr)
                {
                    *error = "suspensionStrokeMeters must contain 24 values";
                }
                return false;
            }
            for (size_t index = 0; index < loaded.suspensionStrokeMeters.size(); ++index)
            {
                if (!(*suspensionStrokes)[index].is_number())
                {
                    if (error != nullptr)
                    {
                        *error = "suspensionStrokeMeters values must be numbers";
                    }
                    return false;
                }
                loaded.suspensionStrokeMeters[index] =
                    (*suspensionStrokes)[index].get<float>();
            }
        }
        ReadBool(json, "neutralBrakeEnabled", loaded.neutralBrakeEnabled);
        ReadFloat(json, "neutralBrakeAmount", loaded.neutralBrakeAmount);
        ReadFloat(
            json,
            "stationaryTurnInnerTrackRatio",
            loaded.stationaryTurnInnerTrackRatio);
        ReadFloat(json, "stationaryTurnLeftTraction", loaded.stationaryTurnLeftTraction);
        ReadFloat(json, "stationaryTurnRightTraction", loaded.stationaryTurnRightTraction);
        ReadFloat(json, "pivotTurnLeftTraction", loaded.pivotTurnLeftTraction);
        ReadFloat(json, "pivotTurnRightTraction", loaded.pivotTurnRightTraction);
        ReadFloat(json, "engineMaxTorqueNm", loaded.engineMaxTorqueNm);
        ReadFloat(json, "engineMaxRpm", loaded.engineMaxRpm);
        ReadFloat(
            json,
            "transmissionShiftDownRpm",
            loaded.transmissionShiftDownRpm);
        ReadFloat(json, "transmissionShiftUpRpm", loaded.transmissionShiftUpRpm);
        ReadFloat(
            json,
            "transmissionClutchStrength",
            loaded.transmissionClutchStrength);
        ReadFloat(json, "finalDriveRatio", loaded.finalDriveRatio);
        ReadFloat(
            json,
            "clutchReleaseTimeSeconds",
            loaded.clutchReleaseTimeSeconds);
        ReadFloat(json, "yawSpeedLimitDegrees", loaded.yawSpeedLimitDegrees);
        ReadFloat(json, "yawDamping", loaded.yawDamping);
        ReadBool(json, "startUpsideDown", loaded.startUpsideDown);
        ReadFloat(json, "stoppedEnterLinearSpeedMetersPerSecond", loaded.stoppedEnterLinearSpeedMetersPerSecond);
        ReadFloat(json, "stoppedExitLinearSpeedMetersPerSecond", loaded.stoppedExitLinearSpeedMetersPerSecond);
        ReadFloat(json, "stoppedEnterAngularSpeedRadiansPerSecond", loaded.stoppedEnterAngularSpeedRadiansPerSecond);
        ReadFloat(json, "stoppedExitAngularSpeedRadiansPerSecond", loaded.stoppedExitAngularSpeedRadiansPerSecond);
        ReadFloat(json, "stoppedEnterTrackSlipMetersPerSecond", loaded.stoppedEnterTrackSlipMetersPerSecond);
        ReadFloat(json, "stoppedExitTrackSlipMetersPerSecond", loaded.stoppedExitTrackSlipMetersPerSecond);
        ReadFloat(json, "stoppedEnterSuspensionSpeedMetersPerSecond", loaded.stoppedEnterSuspensionSpeedMetersPerSecond);
        ReadFloat(json, "stoppedExitSuspensionSpeedMetersPerSecond", loaded.stoppedExitSuspensionSpeedMetersPerSecond);
        ReadFloat(json, "stoppedMinimumUpAlignment", loaded.stoppedMinimumUpAlignment);
        ReadFloat(json, "stoppedConfirmSeconds", loaded.stoppedConfirmSeconds);
        settings = loaded;
        if (error != nullptr)
        {
            error->clear();
        }
        return true;
    }
}
