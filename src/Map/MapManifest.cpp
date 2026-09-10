#include "MapManifest.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <limits>
#include <set>
#include <stdexcept>

namespace Tank::Map
{
    namespace
    {
        using Json = nlohmann::json;

        void Require(bool valid, const std::string& message)
        {
            if (!valid)
            {
                throw std::runtime_error(message);
            }
        }

        std::array<float, 3> ReadVector(const Json& value, const std::string& field)
        {
            Require(value.is_array() && value.size() == 3, field + " must contain XYZ");
            std::array<float, 3> result;
            for (size_t axis = 0; axis < result.size(); ++axis)
            {
                Require(value[axis].is_number(), field + " must contain numbers");
                const double component = value[axis].get<double>();
                Require(std::isfinite(component) &&
                    std::abs(component) <= static_cast<double>((std::numeric_limits<float>::max)()),
                    field + " must contain finite float values");
                result[axis] = static_cast<float>(component);
            }
            return result;
        }

        Transform ReadTransform(const Json& value)
        {
            return { ReadVector(value.at("position"), "position"),
                ReadVector(value.at("rotationDegrees"), "rotationDegrees") };
        }

        std::string ReadId(const Json& entry, std::set<std::string>& ids)
        {
            auto id = entry.at("id").get<std::string>();
            Require(!id.empty() && ids.insert(id).second, "IDs must be nonempty and unique");
            return id;
        }

        bool ValidAssetPath(const std::string& path)
        {
            if (path.empty() || path.front() == '/' ||
                path.find_first_of("\\:") != std::string::npos ||
                std::any_of(path.begin(), path.end(), [](unsigned char c) { return c < 32; }))
            {
                return false;
            }
            size_t start = 0;
            while (start < path.size())
            {
                const size_t end = path.find('/', start);
                const auto part = path.substr(start, end == std::string::npos ? end : end - start);
                if (part.empty() || part == "." || part == "..")
                {
                    return false;
                }
                if (end == std::string::npos)
                {
                    break;
                }
                start = end + 1;
            }
            auto extension = path.substr(path.find_last_of('.') == std::string::npos
                ? path.size() : path.find_last_of('.'));
            std::transform(extension.begin(), extension.end(), extension.begin(),
                [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            return extension == ".gltf" || extension == ".glb";
        }
    }

    bool DeserializeManifest(const std::string& jsonText, Manifest& manifest, std::string& error)
    {
        try
        {
            const Json json = Json::parse(jsonText);
            Require(json.is_object(), "Manifest must be an object");
            Require(json.at("version").is_number_integer() && json.at("version") == 1,
                "Unsupported manifest version");
            Require(json.at("instances").is_array(), "instances must be an array");
            Require(json.at("clearAreas").is_array(), "clearAreas must be an array");
            Manifest loaded;
            loaded.playerSpawn = ReadTransform(json.at("playerSpawn"));
            std::set<std::string> ids;
            for (const auto& entry : json.at("instances"))
            {
                Instance instance;
                instance.id = ReadId(entry, ids);
                instance.asset = entry.at("asset").get<std::string>();
                Require(ValidAssetPath(instance.asset), "asset must be a relative glTF path inside the map folder");
                instance.transform = ReadTransform(entry);
                loaded.instances.push_back(std::move(instance));
            }
            for (const auto& entry : json.at("clearAreas"))
            {
                ClearArea area;
                area.id = ReadId(entry, ids);
                area.name = entry.at("name").get<std::string>();
                area.center = ReadVector(entry.at("center"), "center");
                area.size = ReadVector(entry.at("size"), "size");
                Require(std::all_of(area.size.begin(), area.size.end(),
                    [](float size) { return size > 0.0f; }), "AABB size must be positive");
                loaded.clearAreas.push_back(std::move(area));
            }
            manifest = std::move(loaded);
            error.clear();
            return true;
        }
        catch (const std::exception& exception)
        {
            error = exception.what();
            return false;
        }
    }

    bool SerializeManifest(const Manifest& manifest, std::string& jsonText, std::string& error)
    {
        try
        {
            Json json = {
                { "version", 1 },
                { "playerSpawn", {
                    { "position", manifest.playerSpawn.position },
                    { "rotationDegrees", manifest.playerSpawn.rotationDegrees } } },
                { "instances", Json::array() },
                { "clearAreas", Json::array() }
            };
            for (const auto& instance : manifest.instances)
            {
                json["instances"].push_back({
                    { "id", instance.id }, { "asset", instance.asset },
                    { "position", instance.transform.position },
                    { "rotationDegrees", instance.transform.rotationDegrees } });
            }
            for (const auto& area : manifest.clearAreas)
            {
                json["clearAreas"].push_back({
                    { "id", area.id }, { "name", area.name },
                    { "center", area.center }, { "size", area.size } });
            }
            auto serialized = json.dump(2);
            Manifest validated;
            if (!DeserializeManifest(serialized, validated, error))
            {
                return false;
            }
            jsonText = std::move(serialized);
            error.clear();
            return true;
        }
        catch (const std::exception& exception)
        {
            error = exception.what();
            return false;
        }
    }
}
