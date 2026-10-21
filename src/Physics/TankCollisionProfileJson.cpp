#include "TankCollisionProfileJson.h"

#include <nlohmann/json.hpp>

namespace Tank::Physics
{
    namespace
    {
        const char* ShapeName(TankShapeKind kind)
        {
            return kind == TankShapeKind::Capsule ? "capsule" : "box";
        }

        nlohmann::json SerializeShape(const TankShapeSettings& shape)
        {
            return {
                {"kind", ShapeName(shape.kind)},
                {"position", {shape.localPosition.x, shape.localPosition.y,
                    shape.localPosition.z}},
                {"rotationDegrees", {shape.localRotationDegrees.x,
                    shape.localRotationDegrees.y,
                    shape.localRotationDegrees.z}},
                {"sizeMeters", {shape.sizeMeters.x, shape.sizeMeters.y,
                    shape.sizeMeters.z}}
            };
        }

        bool ReadVec3(const nlohmann::json& object, const char* key,
            Vec3& value, std::string& error)
        {
            if (!object.contains(key)) return true;
            const auto& array = object.at(key);
            if (!array.is_array() || array.size() != 3 ||
                !array[0].is_number() || !array[1].is_number() ||
                !array[2].is_number())
            {
                error = std::string(key) + " must contain three numbers";
                return false;
            }
            value = {array[0].get<float>(), array[1].get<float>(),
                array[2].get<float>()};
            return true;
        }

        bool DeserializeShape(const nlohmann::json& json,
            TankShapeSettings& shape, std::string& error)
        {
            if (!json.is_object())
            {
                error = "tank shape must be an object";
                return false;
            }
            if (json.contains("kind"))
            {
                if (!json.at("kind").is_string())
                {
                    error = "tank shape kind must be a string";
                    return false;
                }
                const auto kind = json.at("kind").get<std::string>();
                if (kind == "box") shape.kind = TankShapeKind::Box;
                else if (kind == "capsule") shape.kind = TankShapeKind::Capsule;
                else
                {
                    error = "unknown tank shape kind: " + kind;
                    return false;
                }
            }
            if (!ReadVec3(json, "position", shape.localPosition, error) ||
                !ReadVec3(json, "rotationDegrees", shape.localRotationDegrees,
                    error) ||
                !ReadVec3(json, "sizeMeters", shape.sizeMeters, error))
            {
                return false;
            }
            if (!IsValidTankShape(shape))
            {
                error = "invalid tank shape dimensions or transform";
                return false;
            }
            return true;
        }
    }

    nlohmann::json SerializeTankCollisionProfile(
        const TankCollisionProfile& profile)
    {
        nlohmann::json result = {
            {"movementShape", SerializeShape(profile.movementShape)},
            {"hitRegions", nlohmann::json::array()}
        };
        for (const auto& region : profile.hitRegions)
        {
            auto entry = SerializeShape(region.shape);
            entry["name"] = region.name;
            result["hitRegions"].push_back(std::move(entry));
        }
        return result;
    }

    bool DeserializeTankCollisionProfile(const nlohmann::json& json,
        TankCollisionProfile& profile, std::string& error)
    {
        if (!json.is_object() || !json.contains("movementShape") ||
            !json.contains("hitRegions") || !json.at("hitRegions").is_array())
        {
            error = "collisionProfile requires movementShape and hitRegions";
            return false;
        }
        TankCollisionProfile loaded = profile;
        if (!DeserializeShape(json.at("movementShape"), loaded.movementShape,
                error))
        {
            return false;
        }
        loaded.hitRegions.clear();
        for (const auto& entry : json.at("hitRegions"))
        {
            if (!entry.is_object() || !entry.contains("name") ||
                !entry.at("name").is_string())
            {
                error = "hit region requires a name";
                return false;
            }
            TankHitRegion region;
            region.name = entry.at("name").get<std::string>();
            if (!DeserializeShape(entry, region.shape, error)) return false;
            loaded.hitRegions.push_back(std::move(region));
        }
        if (!IsValidTankCollisionProfile(loaded))
        {
            error = "invalid tank collision profile";
            return false;
        }
        profile = std::move(loaded);
        return true;
    }
}
