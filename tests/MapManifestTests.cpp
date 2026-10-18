#include "Map/MapManifest.h"

#include <nlohmann/json.hpp>

#include <iostream>
#include <limits>
#include <stdexcept>

namespace
{
    void Check(bool condition, const char* message)
    {
        if (!condition)
        {
            throw std::runtime_error(message);
        }
    }
}

int main()
{
    using namespace Tank::Map;
    using Json = nlohmann::json;
    try
    {
        Manifest source;
        source.playerSpawn = { { 3.0f, 2.5f, -4.0f }, { 10.0f, 90.0f, -15.0f } };
        source.instances = {
            { "building-001", "Models/Building.glb", { { 10.0f, 0.0f, 20.0f },
                { 5.0f, 90.0f, -20.0f }, { 1.75f, 0.8f, 2.3f } } },
            { "building-002", "Models/Building.glb", { { -10.0f, 0.0f, 20.0f }, {} } },
            { "ramp-001", "Ramp.gltf", {} }
        };
        source.clearAreas = {
            { "goal-001", "Goal", { 0.0f, 2.0f, 50.0f }, { 10.0f, 4.0f, 10.0f } },
            { "goal-002", "Second goal", { 20.0f, 2.0f, 50.0f }, { 5.0f, 4.0f, 5.0f } }
        };
        source.enemies = {
            { "enemy-001", "Type 0", { 12.0f, 1.0f, -25.0f }, { 0.0f, 135.0f, 0.0f } },
            { "enemy-002", "Type 0", { -12.0f, 1.0f, 25.0f }, {} }
        };
        source.enemyGroups = { { "patrol-001", { { "enemy-001", 20 }, { "enemy-002", 10 } } } };
        source.enemyGroups[0].patrol = EnemyPatrolSettings{
            {{{10, 0, 0}, "ground"}, {{20, 5, 0}, "bridge"}}, 5, 7, 1, 3};
        std::string error;
        std::string text;
        Check(SerializeManifest(source, text, error), "Serialize populated map");
        Manifest loaded;
        Check(DeserializeManifest(text, loaded, error), "Load populated map");
        std::string roundTrip;
        Check(SerializeManifest(loaded, roundTrip, error) && roundTrip == text, "All fields survive round trip");
        Check(loaded.instances.size() == 3 && loaded.clearAreas.size() == 2,
            "Repeated assets and multiple areas are retained");
        Check(loaded.instances[0].transform.scale == source.instances[0].transform.scale,
            "Per-axis instance scale survives round trip");

        Check(loaded.enemies.size() == 2 && loaded.enemies[0].unitType == "Type 0" &&
            loaded.enemies[0].position == source.enemies[0].position &&
            loaded.enemies[0].rotationDegrees == source.enemies[0].rotationDegrees,
            "Enemy placement fields survive round trip");
        Check(loaded.enemyGroups.size() == 1 && loaded.enemyGroups[0].members.size() == 2 &&
            loaded.enemyGroups[0].members[1].commandPriority == 10,
            "Group membership and priorities survive round trip");
        const Json valid = Json::parse(text);
        auto Reject = [&](const std::string& invalid)
        {
            Check(!DeserializeManifest(invalid, loaded, error) && !error.empty(), "Invalid input rejected with error");
            std::string after;
            Check(SerializeManifest(loaded, after, error) && after == text, "Failure preserves the entire previous map");
        };
        Check(loaded.enemyGroups[0].patrol && loaded.enemyGroups[0].patrol->points[1].surfaceId == "bridge" &&
            loaded.enemyGroups[0].patrol->searchWaitSeconds == 3, "Patrol settings survive round trip");
        for (const Json settings : {Json(nullptr), Json::array(), Json(3)})
        {
            auto bad = valid;
            bad["enemyGroups"][0]["patrol"] = settings;
            Reject(bad.dump());
        }
        for (const char* field : {"attackStopMeters", "resumeApproachMeters", "arrivalMeters", "searchWaitSeconds", "points"})
        {
            auto bad = valid;
            bad["enemyGroups"][0]["patrol"].erase(field);
            Reject(bad.dump());
        }
        for (const Json value : {Json(-1), Json("5"), Json(1e100)})
        {
            auto bad = valid;
            bad["enemyGroups"][0]["patrol"]["attackStopMeters"] = value;
            Reject(bad.dump());
        }
        auto invalidPatrol = valid;
        invalidPatrol["enemyGroups"][0]["patrol"]["resumeApproachMeters"] = 5;
        Reject(invalidPatrol.dump());
        invalidPatrol = valid;
        invalidPatrol["enemyGroups"][0]["patrol"]["points"] = Json::array();
        Reject(invalidPatrol.dump());
        invalidPatrol = valid;
        invalidPatrol["enemyGroups"][0]["patrol"]["points"][0]["surfaceId"] = "";
        Reject(invalidPatrol.dump());
        auto groupWithoutPatrol = valid;
        groupWithoutPatrol["enemyGroups"][0].erase("patrol");
        Manifest oldGroup;
        Check(DeserializeManifest(groupWithoutPatrol.dump(), oldGroup, error) && !oldGroup.enemyGroups[0].patrol,
            "Existing group stays without behavior unless explicitly configured");
        for (const Json groups : { Json(nullptr), Json::object(), Json(3) })
        {
            auto bad = valid;
            bad["enemyGroups"] = groups;
            Reject(bad.dump());
        }
        for (const char* id : { "", "enemy-001", "building-001" })
        {
            auto bad = valid;
            bad["enemyGroups"][0]["id"] = id;
            Reject(bad.dump());
        }
        for (const char* id : { "missing", "building-001", "enemy-001" })
        {
            auto bad = valid;
            bad["enemyGroups"][0]["members"][1]["enemyId"] = id;
            Reject(bad.dump());
        }
        for (const Json priority : { Json(-1), Json(20), Json(1.5), Json("1"), Json(2147483648ULL) })
        {
            auto bad = valid;
            bad["enemyGroups"][0]["members"][1]["commandPriority"] = priority;
            Reject(bad.dump());
        }
        auto emptyGroup = valid;
        emptyGroup["enemyGroups"][0]["members"] = Json::array();
        Reject(emptyGroup.dump());
        auto repeatedGroup = valid;
        repeatedGroup["enemyGroups"].push_back(valid["enemyGroups"][0]);
        repeatedGroup["enemyGroups"][1]["id"] = "other-group";
        Reject(repeatedGroup.dump());
        auto legacyGroups = valid;
        legacyGroups.erase("enemyGroups");
        Manifest ungrouped;
        Check(DeserializeManifest(legacyGroups.dump(), ungrouped, error) && ungrouped.enemyGroups.empty() &&
            ungrouped.enemies.size() == 2, "Legacy enemies remain ungrouped");
        auto invalidGroup = source;
        invalidGroup.enemyGroups[0].members[0].enemyId = "missing";
        std::string unchangedGroup = "previous";
        Check(!SerializeManifest(invalidGroup, unchangedGroup, error) && unchangedGroup == "previous",
            "Invalid group serialization preserves output");
        for (const Json enemies : { Json(nullptr), Json::object(), Json(3) })
        {
            auto bad = valid;
            bad["enemies"] = enemies;
            Reject(bad.dump());
        }
        for (const char* field : { "id", "unitType", "position", "rotationDegrees" })
        {
            auto bad = valid;
            bad["enemies"][0].erase(field);
            Reject(bad.dump());
        }
        for (const Json type : { Json(""), Json(nullptr), Json(1) })
        {
            auto bad = valid;
            bad["enemies"][0]["unitType"] = type;
            Reject(bad.dump());
        }
        for (const char* id : { "", "enemy-001", "building-001", "goal-001" })
        {
            auto bad = valid;
            bad["enemies"][1]["id"] = id;
            Reject(bad.dump());
        }
        for (const char* field : { "position", "rotationDegrees" })
        {
            for (const Json vector : { Json::array({ 0, 0 }), Json::array({ 0, "1", 2 }),
                Json::array({ 0, 1e100, 2 }) })
            {
                auto bad = valid;
                bad["enemies"][0][field] = vector;
                Reject(bad.dump());
            }
        }
        Reject("{");
        Reject("[]");
        Reject("{}");
        for (const Json version : { Json(2), Json(1.0), Json("1"), Json(4294967297ULL) })
        {
            auto bad = valid;
            bad["version"] = version;
            Reject(bad.dump());
        }
        for (const char* key : { "playerSpawn", "instances", "clearAreas" })
        {
            auto bad = valid;
            bad.erase(key);
            Reject(bad.dump());
            bad = valid;
            bad[key] = nullptr;
            Reject(bad.dump());
        }
        for (const Json vector : { Json::array({ 0, 0 }), Json::array({ 0, "1", 2 }),
            Json::array({ 0, nullptr, 2 }), Json::array({ 0, 1e100, 2 }) })
        {
            auto bad = valid;
            bad["playerSpawn"]["position"] = vector;
            Reject(bad.dump());
            bad = valid;
            bad["instances"][0]["rotationDegrees"] = vector;
            Reject(bad.dump());
        }
        for (const char* path : { "../Other.glb", "/Other.glb", "C:/Other.glb", "C:Other.glb",
            "Models/../Other.glb", "Models\\Other.glb", "Models//Other.glb", "Other.obj", "" })
        {
            auto bad = valid;
            bad["instances"][0]["asset"] = path;
            Reject(bad.dump());
        }
        for (const float size : { 0.0f, -1.0f })
        {
            auto bad = valid;
            bad["clearAreas"][1]["size"][2] = size;
            Reject(bad.dump());
        }
        for (const char* id : { "", "building-001" })
        {
            auto bad = valid;
            bad["clearAreas"][1]["id"] = id;
            Reject(bad.dump());
            bad = valid;
            bad["instances"][1]["id"] = id;
            Reject(bad.dump());
        }
        for (const Json scale : { Json(0), Json(-1), Json("1"), Json(1e100) })
        {
            auto bad = valid;
            bad["instances"][0]["scale"] = scale;
            Reject(bad.dump());
        }
        for (const Json scale : { Json::array({1.0, 0.0, 1.0}), Json::array({1.0, -1.0, 1.0}),
                 Json::array({1.0, "1", 1.0}), Json::array({1.0, 1e100, 1.0}) })
        {
            auto bad = valid;
            bad["instances"][0]["scale"] = scale;
            Reject(bad.dump());
        }
        auto withoutEnemies = valid;
        withoutEnemies.erase("enemies");
        withoutEnemies.erase("enemyGroups");
        Check(DeserializeManifest(withoutEnemies.dump(), loaded, error) && loaded.enemies.empty(),
            "Legacy manifest clears previous enemies");
        auto invalidEnemy = source;
        invalidEnemy.enemies[0].rotationDegrees[1] = std::numeric_limits<float>::infinity();
        std::string preserved = "previous output";
        Check(!SerializeManifest(invalidEnemy, preserved, error) && preserved == "previous output",
            "Nonfinite enemy rotation preserves saved output");
        auto legacy = valid;
        legacy["instances"][0].erase("scale");
        Check(DeserializeManifest(legacy.dump(), loaded, error) &&
            loaded.instances[0].transform.scale == std::array<float, 3>{1.0f, 1.0f, 1.0f},
            "Manifest without scale keeps the unit-scale default");
        legacy = valid;
        legacy["instances"][0]["scale"] = 1.75;
        Check(DeserializeManifest(legacy.dump(), loaded, error) &&
            loaded.instances[0].transform.scale == std::array<float, 3>{1.75f, 1.75f, 1.75f},
            "Legacy scalar scale is expanded uniformly to XYZ");
        auto invalid = source;
        invalid.playerSpawn.position[0] = std::numeric_limits<float>::infinity();
        std::string unchanged = "previous output";
        Check(!SerializeManifest(invalid, unchanged, error) && unchanged == "previous output" && !error.empty(),
            "Invalid in-memory data must not produce a saved manifest");
        Check(SerializeManifest({}, roundTrip, error) && DeserializeManifest(roundTrip, loaded, error) &&
            loaded.instances.empty() && loaded.clearAreas.empty() && loaded.enemies.empty() && loaded.playerSpawn.position[1] == 2.0f &&
            error.empty(), "New empty map clears previous state and errors");
        std::cout << "PASS MapManifest\n";
        return 0;
    }
    catch (const std::exception& exception)
    {
        std::cerr << "FAIL MapManifest: " << exception.what() << '\n';
        return 1;
    }
}
