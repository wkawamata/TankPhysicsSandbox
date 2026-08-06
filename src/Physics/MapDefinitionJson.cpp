#include "MapDefinitionJson.h"

#include "PhysicsEnvironmentSettingsJson.h"

#include <nlohmann/json.hpp>

#include <cmath>

namespace Tank::Physics
{
    namespace
    {
        constexpr int kSchemaVersion = 3;

        const char* ShapeName(MapPrimitiveType type)
        {
            if (type == MapPrimitiveType::TriangularPrism)
            {
                return "triangularPrism";
            }
            return type == MapPrimitiveType::HeightField ? "heightField" : "box";
        }

        bool ReadVec3(const nlohmann::json& json, Vec3& value)
        {
            if (!json.is_array() || json.size() != 3 ||
                !json[0].is_number() || !json[1].is_number() ||
                !json[2].is_number())
            {
                return false;
            }
            value = { json[0].get<float>(), json[1].get<float>(), json[2].get<float>() };
            return std::isfinite(value.x) && std::isfinite(value.y) &&
                std::isfinite(value.z);
        }

        bool Fail(std::string* error, const char* message)
        {
            if (error != nullptr)
            {
                *error = message;
            }
            return false;
        }
    }

    std::string SerializeMapDocument(const MapDocument& document)
    {
        nlohmann::json json;
        json["version"] = kSchemaVersion;
        json["name"] = document.name;
        json["environment"] = nlohmann::json::parse(
            SerializePhysicsEnvironmentSettings(document.environment));
        json["spawn"] = {
            { "position", {
                document.spawn.position.x,
                document.spawn.position.y,
                document.spawn.position.z } },
            { "yawRadians", document.spawn.yawRadians },
        };
        json["primitives"] = nlohmann::json::array();
        for (const MapPrimitive& primitive : document.primitives)
        {
            nlohmann::json entry = {
                { "shape", ShapeName(primitive.type) },
                { "position", { primitive.position.x, primitive.position.y, primitive.position.z } },
                { "size", { primitive.size.x, primitive.size.y, primitive.size.z } },
                { "yawRadians", primitive.yawRadians },
                { "friction", primitive.friction },
            };
            if (primitive.type == MapPrimitiveType::HeightField)
            {
                entry["sampleCount"] = primitive.heightFieldSampleCount;
                entry["cellSizeM"] = primitive.heightFieldCellSizeM;
                entry["heights"] = primitive.heightFieldHeights;
            }
            json["primitives"].push_back(std::move(entry));
        }
        return json.dump(2);
    }

    bool DeserializeMapDocument(
        const std::string& jsonText,
        MapDocument& document,
        std::string* error)
    {
        const nlohmann::json json = nlohmann::json::parse(jsonText, nullptr, false);
        if (json.is_discarded() || !json.is_object())
        {
            return Fail(error, "invalid JSON");
        }
        const auto version = json.find("version");
        if (version == json.end() || !version->is_number_integer() ||
            version->get<int>() < 1 || version->get<int>() > kSchemaVersion)
        {
            return Fail(error, "unsupported version");
        }
        const auto name = json.find("name");
        const auto environment = json.find("environment");
        const auto primitives = json.find("primitives");
        if (name == json.end() || !name->is_string() || name->get<std::string>().empty() ||
            environment == json.end() || !environment->is_object() ||
            primitives == json.end() || !primitives->is_array())
        {
            return Fail(error, "map fields are invalid");
        }

        MapDocument loaded = document;
        loaded.name = name->get<std::string>();
        if (!DeserializePhysicsEnvironmentSettings(
                environment->dump(), loaded.environment, error))
        {
            return false;
        }
        const auto spawn = json.find("spawn");
        if (spawn != json.end())
        {
            if (!spawn->is_object())
            {
                return Fail(error, "spawn must be an object");
            }
            const auto position = spawn->find("position");
            const auto yaw = spawn->find("yawRadians");
            if (position == spawn->end() || !ReadVec3(*position, loaded.spawn.position) ||
                yaw == spawn->end() || !yaw->is_number())
            {
                return Fail(error, "spawn fields are invalid");
            }
            loaded.spawn.yawRadians = yaw->get<float>();
            if (!std::isfinite(loaded.spawn.yawRadians))
            {
                return Fail(error, "spawn yaw is invalid");
            }
        }
        loaded.primitives.clear();
        loaded.primitives.reserve(primitives->size());
        for (const nlohmann::json& entry : *primitives)
        {
            if (!entry.is_object())
            {
                return Fail(error, "primitive must be an object");
            }
            const auto shape = entry.find("shape");
            const auto position = entry.find("position");
            const auto size = entry.find("size");
            const auto yaw = entry.find("yawRadians");
            const auto friction = entry.find("friction");
            if (shape == entry.end() || !shape->is_string() ||
                position == entry.end() || size == entry.end() ||
                yaw == entry.end() || !yaw->is_number() ||
                friction == entry.end() || !friction->is_number())
            {
                return Fail(error, "primitive fields are invalid");
            }

            MapPrimitive primitive;
            const std::string shapeName = shape->get<std::string>();
            if (shapeName == "box")
            {
                primitive.type = MapPrimitiveType::Box;
            }
            else if (shapeName == "triangularPrism")
            {
                primitive.type = MapPrimitiveType::TriangularPrism;
            }
            else if (shapeName == "heightField")
            {
                primitive.type = MapPrimitiveType::HeightField;
            }
            else
            {
                return Fail(error, "unknown primitive shape");
            }
            if (!ReadVec3(*position, primitive.position) ||
                !ReadVec3(*size, primitive.size))
            {
                return Fail(error, "primitive vectors are invalid");
            }
            primitive.yawRadians = yaw->get<float>();
            primitive.friction = friction->get<float>();
            if (!std::isfinite(primitive.yawRadians) ||
                !std::isfinite(primitive.friction) || primitive.friction < 0.0f ||
                primitive.friction > 2.0f)
            {
                return Fail(error, "primitive values are out of range");
            }
            if (primitive.type == MapPrimitiveType::HeightField)
            {
                const auto sampleCount = entry.find("sampleCount");
                const auto cellSize = entry.find("cellSizeM");
                const auto heights = entry.find("heights");
                if (sampleCount == entry.end() || !sampleCount->is_number_unsigned() ||
                    cellSize == entry.end() || !cellSize->is_number() ||
                    heights == entry.end() || !heights->is_array())
                {
                    return Fail(error, "height field fields are invalid");
                }
                primitive.heightFieldSampleCount = sampleCount->get<uint32_t>();
                primitive.heightFieldCellSizeM = cellSize->get<float>();
                if (primitive.heightFieldSampleCount < 2 ||
                    primitive.heightFieldSampleCount > 256 ||
                    !std::isfinite(primitive.heightFieldCellSizeM) ||
                    primitive.heightFieldCellSizeM <= 0.0f ||
                    heights->size() != static_cast<size_t>(primitive.heightFieldSampleCount) *
                        primitive.heightFieldSampleCount)
                {
                    return Fail(error, "height field dimensions are invalid");
                }
                primitive.heightFieldHeights.reserve(heights->size());
                for (const nlohmann::json& height : *heights)
                {
                    if (!height.is_number())
                    {
                        return Fail(error, "height field sample is invalid");
                    }
                    const float value = height.get<float>();
                    if (!std::isfinite(value))
                    {
                        return Fail(error, "height field sample is invalid");
                    }
                    primitive.heightFieldHeights.push_back(value);
                }
            }
            else if (primitive.size.x <= 0.0f || primitive.size.y <= 0.0f ||
                     primitive.size.z <= 0.0f)
            {
                return Fail(error, "primitive size is out of range");
            }
            loaded.primitives.push_back(primitive);
        }

        document = std::move(loaded);
        if (error != nullptr)
        {
            error->clear();
        }
        return true;
    }
}
