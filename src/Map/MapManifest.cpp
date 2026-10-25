#include "MapManifest.h"
#include "MapHierarchy.h"

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

        std::array<float, 3> ReadScale(const Json& value)
        {
            std::array<float, 3> result{};
            if (value.is_number())
            {
                const double uniformScale = value.get<double>();
                Require(std::isfinite(uniformScale) && uniformScale > 0.0 &&
                    uniformScale <= static_cast<double>((std::numeric_limits<float>::max)()),
                    "scale must be a positive finite float or XYZ array");
                result.fill(static_cast<float>(uniformScale));
                return result;
            }

            result = ReadVector(value, "scale");
            Require(std::all_of(result.begin(), result.end(), [](float component) { return component > 0.0f; }),
                "scale XYZ components must be positive");
            return result;
        }

        Transform ReadTransform(const Json& value)
        {
            Transform transform = {
                ReadVector(value.at("position"), "position"),
                ReadVector(value.at("rotationDegrees"), "rotationDegrees") };
            if (const auto scale = value.find("scale"); scale != value.end())
                transform.scale = ReadScale(*scale);
            return transform;
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

    bool IsValidMapName(const std::string& name, std::string& error)
    {
        if (name.empty() || name.size() > 256 ||
            std::isspace(static_cast<unsigned char>(name.front())) ||
            std::isspace(static_cast<unsigned char>(name.back())) ||
            std::any_of(name.begin(), name.end(), [](unsigned char c) { return c < 32 || c == 127; }))
        {
            error = "Map name must be 1-256 UTF-8 bytes, without control characters or surrounding whitespace.";
            return false;
        }
        error.clear();
        return true;
    }

    bool IsValidEnemyPatrolSettings(const EnemyPatrolSettings& settings)
    {
        return !settings.points.empty() &&
            std::isfinite(settings.attackStopMeters) && settings.attackStopMeters > 0 &&
            std::isfinite(settings.resumeApproachMeters) && settings.resumeApproachMeters > settings.attackStopMeters &&
            std::isfinite(settings.arrivalMeters) && settings.arrivalMeters > 0 &&
            std::isfinite(settings.searchWaitSeconds) && settings.searchWaitSeconds >= 0 &&
            std::all_of(settings.points.begin(), settings.points.end(), [](const auto& point)
            {
                return !point.surfaceId.empty() && std::all_of(point.position.begin(), point.position.end(),
                    [](float value) { return std::isfinite(value); });
            });
    }

    bool DeserializeManifest(const std::string& jsonText, Manifest& manifest, std::string& error)
    {
        try
        {
            const Json json = Json::parse(jsonText);
            Require(json.is_object(), "Manifest must be an object");
            Require(json.at("version").is_number_integer() && (json.at("version") == 1 || json.at("version") == 2),
                "Unsupported manifest version");
            Require(json.at("instances").is_array(), "instances must be an array");
            Require(json.at("clearAreas").is_array(), "clearAreas must be an array");
            Manifest loaded;
            if (const auto name = json.find("name"); name != json.end())
            {
                Require(name->is_string(), "Map name must be a string");
                loaded.name = name->get<std::string>();
                Require(IsValidMapName(loaded.name, error), error);
            }
            if (const auto playable = json.find("playable"); playable != json.end())
            {
                Require(playable->is_boolean(), "playable must be a boolean");
                loaded.playable = playable->get<bool>();
            }
            loaded.playerSpawn = ReadTransform(json.at("playerSpawn"));
            loaded.playerSpawnParentId = json.at("playerSpawn").value("parentId", std::string("root"));
            std::set<std::string> ids;
            for (const auto& entry : json.at("instances"))
            {
                Instance instance;
                instance.id = ReadId(entry, ids);
                instance.asset = entry.at("asset").get<std::string>();
                Require(ValidAssetPath(instance.asset), "asset must be a relative glTF path inside the map folder");
                instance.transform = ReadTransform(entry);
                instance.parentId = entry.value("parentId", std::string("root"));
                instance.breakable = entry.value("breakable", false);
                instance.hitPoints = entry.value("hitPoints", 60.0f);
                Require(std::isfinite(instance.hitPoints) && instance.hitPoints > 0, "Mesh hitPoints must be positive and finite");
                if (entry.contains("gate"))
                {
                    const auto& gate = entry.at("gate");
                    instance.gate = GateSettings{gate.at("triggerRadiusMeters").get<float>(),
                        gate.at("travelMeters").get<float>(), gate.value("openingSpeedMetersPerSecond", 2.0f)};
                    const auto& g = *instance.gate;
                    Require(!instance.breakable && std::isfinite(g.triggerRadiusMeters) && g.triggerRadiusMeters > 0 &&
                        std::isfinite(g.travelMeters) && g.travelMeters > 0 &&
                        std::isfinite(g.openingSpeedMetersPerSecond) && g.openingSpeedMetersPerSecond > 0,
                        "Gate must be indestructible with positive finite radius, travel and speed");
                }
                loaded.instances.push_back(std::move(instance));
            }
            for (const auto& entry : json.at("clearAreas"))
            {
                ClearArea area;
                area.id = ReadId(entry, ids);
                area.name = entry.at("name").get<std::string>();
                area.parentId = entry.value("parentId", std::string("root"));
                if (entry.contains("position") || entry.contains("scale") || entry.contains("rotationDegrees"))
                {
                    const auto transform = ReadTransform(entry);
                    area.center = transform.position; area.size = transform.scale; area.rotationDegrees = transform.rotationDegrees;
                }
                else
                {
                    area.center = ReadVector(entry.at("center"), "center");
                    area.size = ReadVector(entry.at("size"), "size");
                }
                Require(std::all_of(area.size.begin(), area.size.end(),
                    [](float size) { return size > 0.0f; }), "Clear Area scale must be positive");
                loaded.clearAreas.push_back(std::move(area));
            }
            if (const auto enemies = json.find("enemies"); enemies != json.end())
            {
                Require(enemies->is_array(), "enemies must be an array");
                for (const auto& entry : *enemies)
                {
                    EnemyPlacement enemy;
                    enemy.id = ReadId(entry, ids);
                    enemy.unitType = entry.at("unitType").get<std::string>();
                    enemy.parentId = entry.value("parentId", std::string("root"));
                    enemy.supportedByParent = entry.value("supportedByParent", false);
                    Require(!enemy.unitType.empty(), "enemy unitType must be nonempty");
                    enemy.position = ReadVector(entry.at("position"), "enemy position");
                    enemy.rotationDegrees = ReadVector(entry.at("rotationDegrees"), "enemy rotationDegrees");
                    loaded.enemies.push_back(std::move(enemy));
                }
            }
            if (const auto groups = json.find("enemyGroups"); groups != json.end())
            {
                Require(groups->is_array(), "enemyGroups must be an array");
                std::set<std::string> groupedEnemies;
                for (const auto& entry : *groups)
                {
                    EnemyGroup group;
                    group.id = ReadId(entry, ids);
                    const auto& members = entry.at("members");
                    Require(members.is_array() && !members.empty(), "Group members must be a nonempty array");
                    std::set<int> priorities;
                    for (const auto& member : members)
                    {
                        EnemyGroupMember value;
                        value.enemyId = member.at("enemyId").get<std::string>();
                        Require(std::any_of(loaded.enemies.begin(), loaded.enemies.end(),
                            [&](const auto& enemy) { return enemy.id == value.enemyId; }),
                            "Group member must reference an enemy placement");
                        Require(groupedEnemies.insert(value.enemyId).second,
                            "Enemy must belong to at most one group");
                        const auto& priority = member.at("commandPriority");
                        Require(priority.is_number_integer(), "commandPriority must be an integer");
                        const double number = priority.get<double>();
                        Require(number >= 0 && number <= (std::numeric_limits<int>::max)(),
                            "commandPriority must be a nonnegative int");
                        value.commandPriority = priority.get<int>();
                        Require(priorities.insert(value.commandPriority).second,
                            "Command priorities must be unique within the group");
                        group.members.push_back(std::move(value));
                    }
                    if (const auto patrol = entry.find("patrol"); patrol != entry.end())
                    {
                        Require(patrol->is_object(), "patrol must be an object");
                        EnemyPatrolSettings settings;
                        const auto number = [&](const char* name)
                        {
                            const auto& value = patrol->at(name);
                            Require(value.is_number(), std::string(name) + " must be a number");
                            const double result = value.get<double>();
                            Require(std::isfinite(result) && std::abs(result) <= (std::numeric_limits<float>::max)(),
                                std::string(name) + " must be a finite float");
                            return static_cast<float>(result);
                        };
                        settings.attackStopMeters = number("attackStopMeters");
                        settings.resumeApproachMeters = number("resumeApproachMeters");
                        settings.arrivalMeters = number("arrivalMeters");
                        settings.searchWaitSeconds = number("searchWaitSeconds");
                        const auto& points = patrol->at("points");
                        Require(points.is_array(), "patrol points must be an array");
                        for (const auto& point : points)
                            settings.points.push_back({ReadVector(point.at("position"), "patrol position"),
                                point.at("surfaceId").get<std::string>()});
                        Require(IsValidEnemyPatrolSettings(settings), "Invalid patrol settings");
                        group.patrol = std::move(settings);
                    }
                    loaded.enemyGroups.push_back(std::move(group));
                }
            }
            if (const auto nodes = json.find("nodes"); nodes != json.end())
            {
                Require(nodes->is_array(), "nodes must be an array");
                loaded.nodes.clear();
                for (const auto& entry : *nodes)
                {
                    Require(entry.value("type", std::string()) == "Empty", "Expected an Empty node");
                    loaded.nodes.push_back({entry.at("id").get<std::string>(), entry.at("name").get<std::string>(),
                        entry.at("parentId").get<std::string>(), ReadTransform(entry)});
                }
            }
            if (const auto starts = json.find("startPoints"); starts != json.end())
            {
                Require(starts->is_array(), "startPoints must be an array");
                for (const auto& entry : *starts)
                    loaded.startPoints.push_back({ReadId(entry, ids), ReadTransform(entry),
                        entry.value("parentId", std::string("root"))});
            }
            if (const auto info = json.find("nodeInfo"); info != json.end())
            {
                Require(info->is_object(), "nodeInfo must be an object");
                for (const auto& [id, value] : info->items())
                    loaded.nodeInfo[id] = {value.at("active").get<bool>(), value.at("visible").get<bool>(),
                        value.value("name", std::string{})};
            }
            if (const auto order = json.find("treeOrder"); order != json.end())
            {
                Require(order->is_array(), "treeOrder must be an array");
                std::set<std::string> orderedIds;
                for (const auto& id : *order)
                {
                    const auto value = id.get<std::string>();
                    Require(orderedIds.insert(value).second, "treeOrder IDs must be unique");
                    loaded.treeOrder.push_back(value);
                }
            }
            Require(ValidateHierarchy(loaded, error), error);
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
                { "version", 2 },
                { "playable", manifest.playable },
                { "playerSpawn", {
                    { "parentId", manifest.playerSpawnParentId },
                    { "position", manifest.playerSpawn.position },
                    { "rotationDegrees", manifest.playerSpawn.rotationDegrees },
                    { "scale", manifest.playerSpawn.scale } } },
                { "instances", Json::array() },
                { "clearAreas", Json::array() },
                { "enemies", Json::array() },
                { "nodes", Json::array() },
                { "treeOrder", Json::array() },
                { "startPoints", Json::array() },
                { "nodeInfo", Json::object() }
            };
            if (!manifest.name.empty())
            {
                Require(IsValidMapName(manifest.name, error), error);
                json["name"] = manifest.name;
            }
            for (const auto& node : MapTree(manifest))
            {
                json["treeOrder"].push_back(node.id);
                const auto info = GetNodeInfo(manifest, node.id);
                json["nodeInfo"][node.id] = {{"active", info.active}, {"visible", info.visible}};
                if (!info.name.empty()) json["nodeInfo"][node.id]["name"] = info.name;
            }
            for (const auto& start : manifest.startPoints)
                json["startPoints"].push_back({{"id", start.id}, {"parentId", start.parentId},
                    {"position", start.transform.position}, {"rotationDegrees", start.transform.rotationDegrees},
                    {"scale", start.transform.scale}});
            for (const auto& node : manifest.nodes)
                json["nodes"].push_back({{"id", node.id}, {"name", node.name}, {"type", "Empty"},
                    {"parentId", node.parentId}, {"position", node.transform.position},
                    {"rotationDegrees", node.transform.rotationDegrees}, {"scale", node.transform.scale}});
            for (const auto& instance : manifest.instances)
            {
                Require(std::isfinite(instance.hitPoints) && instance.hitPoints > 0, "Mesh hitPoints must be positive and finite");
                json["instances"].push_back({
                    { "id", instance.id }, { "asset", instance.asset }, { "parentId", instance.parentId },
                    { "position", instance.transform.position },
                    { "rotationDegrees", instance.transform.rotationDegrees },
                    { "scale", instance.transform.scale },
                    { "breakable", instance.breakable }, { "hitPoints", instance.hitPoints } });
                if (instance.gate)
                {
                    const auto& g = *instance.gate;
                    Require(!instance.breakable && std::isfinite(g.triggerRadiusMeters) && g.triggerRadiusMeters > 0 &&
                        std::isfinite(g.travelMeters) && g.travelMeters > 0 &&
                        std::isfinite(g.openingSpeedMetersPerSecond) && g.openingSpeedMetersPerSecond > 0,
                        "Gate must be indestructible with positive finite radius, travel and speed");
                    json["instances"].back()["gate"] = {{"triggerRadiusMeters", g.triggerRadiusMeters},
                        {"travelMeters", g.travelMeters}, {"openingSpeedMetersPerSecond", g.openingSpeedMetersPerSecond}};
                }
            }
            for (const auto& area : manifest.clearAreas)
            {
                json["clearAreas"].push_back({
                    { "id", area.id }, { "name", area.name }, { "parentId", area.parentId },
                    { "position", area.center }, { "rotationDegrees", area.rotationDegrees }, { "scale", area.size } });
            }
            for (const auto& enemy : manifest.enemies)
            {
                json["enemies"].push_back({
                    { "id", enemy.id }, { "unitType", enemy.unitType }, { "parentId", enemy.parentId },
                    { "position", enemy.position },
                    { "rotationDegrees", enemy.rotationDegrees }, { "supportedByParent", enemy.supportedByParent } });
            }
            if (!manifest.enemyGroups.empty())
            {
                json["enemyGroups"] = Json::array();
                for (const auto& group : manifest.enemyGroups)
                {
                    Json members = Json::array();
                    for (const auto& member : group.members)
                        members.push_back({ { "enemyId", member.enemyId },
                            { "commandPriority", member.commandPriority } });
                    Json entry = { { "id", group.id }, { "members", members } };
                    if (group.patrol)
                    {
                        const auto& settings = *group.patrol;
                        Json points = Json::array();
                        for (const auto& point : settings.points)
                            points.push_back({ { "position", point.position }, { "surfaceId", point.surfaceId } });
                        entry["patrol"] = { { "points", points },
                            { "attackStopMeters", settings.attackStopMeters },
                            { "resumeApproachMeters", settings.resumeApproachMeters },
                            { "arrivalMeters", settings.arrivalMeters },
                            { "searchWaitSeconds", settings.searchWaitSeconds } };
                    }
                    json["enemyGroups"].push_back(std::move(entry));
                }
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
