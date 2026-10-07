#include "MortarProfile.h"

#include <nlohmann/json.hpp>

namespace Tank::Physics
{
    namespace
    {
        void ReadFloat(const nlohmann::json& json, const char* name, float& value)
        {
            const auto entry = json.find(name);
            if (entry != json.end() && entry->is_number()) value = entry->get<float>();
        }

        void ReadBool(const nlohmann::json& json, const char* name, bool& value)
        {
            const auto entry = json.find(name);
            if (entry != json.end() && entry->is_boolean()) value = entry->get<bool>();
        }
    }

    MortarProfile ExtractMortarProfile(const TankSettings& s)
    {
        return {s.mortarMinimumFireAngleDegrees, s.mortarMaximumAngleDegrees,
            s.mortarRaiseRateDegreesPerSecond, s.mortarReturnRateDegreesPerSecond,
            s.mortarMinimumRangeMeters, s.mortarMaximumRangeMeters,
            s.mortarMinimumAttackRadiusMeters, s.mortarMaximumAttackRadiusMeters,
            s.mortarStanceTorqueNm, s.mortarStanceDampingNms,
            s.mortarEmergencyBrakeAmount,
            s.mortarMuzzleVelocityMetersPerSecond, s.mortarMuzzleVelocityAuto,
            s.mortarExplosionDamage};
    }

    void ApplyMortarProfile(const MortarProfile& p, TankSettings& s)
    {
        s.mortarMinimumFireAngleDegrees = p.minimumFireAngleDegrees;
        s.mortarMaximumAngleDegrees = p.maximumAngleDegrees;
        s.mortarRaiseRateDegreesPerSecond = p.raiseRateDegreesPerSecond;
        s.mortarReturnRateDegreesPerSecond = p.returnRateDegreesPerSecond;
        s.mortarMinimumRangeMeters = p.minimumRangeMeters;
        s.mortarMaximumRangeMeters = p.maximumRangeMeters;
        s.mortarMinimumAttackRadiusMeters = p.minimumAttackRadiusMeters;
        s.mortarMaximumAttackRadiusMeters = p.maximumAttackRadiusMeters;
        s.mortarStanceTorqueNm = p.stanceTorqueNm;
        s.mortarStanceDampingNms = p.stanceDampingNms;
        s.mortarEmergencyBrakeAmount = p.emergencyBrakeAmount;
        s.mortarMuzzleVelocityMetersPerSecond = p.muzzleVelocityMetersPerSecond;
        s.mortarMuzzleVelocityAuto = p.muzzleVelocityAuto;
        s.mortarExplosionDamage = p.explosionDamage;
    }

    std::string SerializeMortarProfile(const MortarProfile& p)
    {
        nlohmann::json json = {{"version", 1},
            {"minimumFireAngleDegrees", p.minimumFireAngleDegrees},
            {"maximumAngleDegrees", p.maximumAngleDegrees},
            {"raiseRateDegreesPerSecond", p.raiseRateDegreesPerSecond},
            {"returnRateDegreesPerSecond", p.returnRateDegreesPerSecond},
            {"minimumRangeMeters", p.minimumRangeMeters},
            {"maximumRangeMeters", p.maximumRangeMeters},
            {"minimumAttackRadiusMeters", p.minimumAttackRadiusMeters},
            {"maximumAttackRadiusMeters", p.maximumAttackRadiusMeters},
            {"stanceTorqueNm", p.stanceTorqueNm},
            {"stanceDampingNms", p.stanceDampingNms},
            {"emergencyBrakeAmount", p.emergencyBrakeAmount},
            {"muzzleVelocityMetersPerSecond", p.muzzleVelocityMetersPerSecond},
            {"muzzleVelocityAuto", p.muzzleVelocityAuto},
            {"explosionDamage", p.explosionDamage}};
        return json.dump(2);
    }

    bool DeserializeMortarProfile(const std::string& text, MortarProfile& profile,
        std::string* error)
    {
        const nlohmann::json json = nlohmann::json::parse(text, nullptr, false);
        if (json.is_discarded() || !json.is_object()) { if (error) *error = "invalid JSON"; return false; }
        const auto version = json.find("version");
        if (version != json.end() && (!version->is_number_integer() || version->get<int>() != 1))
        { if (error) *error = "unsupported version"; return false; }
        MortarProfile loaded = profile;
        ReadFloat(json, "minimumFireAngleDegrees", loaded.minimumFireAngleDegrees);
        ReadFloat(json, "maximumAngleDegrees", loaded.maximumAngleDegrees);
        ReadFloat(json, "raiseRateDegreesPerSecond", loaded.raiseRateDegreesPerSecond);
        ReadFloat(json, "returnRateDegreesPerSecond", loaded.returnRateDegreesPerSecond);
        ReadFloat(json, "minimumRangeMeters", loaded.minimumRangeMeters);
        ReadFloat(json, "maximumRangeMeters", loaded.maximumRangeMeters);
        ReadFloat(json, "minimumAttackRadiusMeters", loaded.minimumAttackRadiusMeters);
        ReadFloat(json, "maximumAttackRadiusMeters", loaded.maximumAttackRadiusMeters);
        ReadFloat(json, "stanceTorqueNm", loaded.stanceTorqueNm);
        ReadFloat(json, "stanceDampingNms", loaded.stanceDampingNms);
        ReadFloat(json, "emergencyBrakeAmount", loaded.emergencyBrakeAmount);
        ReadFloat(json, "muzzleVelocityMetersPerSecond", loaded.muzzleVelocityMetersPerSecond);
        ReadBool(json, "muzzleVelocityAuto", loaded.muzzleVelocityAuto);
        ReadFloat(json, "explosionDamage", loaded.explosionDamage);
        profile = loaded; if (error) error->clear(); return true;
    }
}
