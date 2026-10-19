#include "Map/MapManifest.h"
#include "Map/MapHierarchy.h"
#include "Map/GltfHitMesh.h"
#include "Ui/SceneTreeUI.h"
#include <cmath>

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
        for (const Json version : { Json(3), Json(1.0), Json("1"), Json(4294967297ULL) })
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
            bad["clearAreas"][1]["scale"][2] = size;
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
        Manifest hierarchy;
        hierarchy.nodes.push_back({"group", "Group", "root", {{10, 0, 0}, {0, 90, 0}, {2, 2, 2}}});
        hierarchy.instances.push_back({"tile", "Models/tile.gltf", {{0, 0, 3}, {}}, "group"});
        hierarchy.enemies.push_back({"tank", "Tank", {1, 0, 0}, {}, "mesh/tile"});
        hierarchy.playerSpawnParentId = "group";
        hierarchy.playerSpawn.position = {0, 1, 0};
        Check(SerializeManifest(hierarchy, roundTrip, error) && DeserializeManifest(roundTrip, loaded, error),
            "Round-trip a hierarchy with typed children and an empty root");
        Check(loaded.instances[0].parentId == "group" && loaded.nodes.size() == 2,
            "Persist parent references and empty nodes");
        Manifest world;
        Check(ResolveHierarchy(loaded, world, error), "Resolve parent transforms");
        Check(std::abs(world.instances[0].transform.position[0] - 16) < 0.001f &&
            std::abs(world.enemies[0].position[2] + 2) < 0.001f &&
            std::abs(world.playerSpawn.position[1] - 2) < 0.001f,
            "Parent rotation and scale affect meshes, enemies and player start");
        Transform recovered;
        Check(NodeTransformFromWorld(loaded, "mesh/tile", *world.instances[0].transform.worldMatrix, recovered, error) &&
            std::abs(recovered.position[2] - 3) < 0.001f,
            "World-space gizmo transform converts back to parent-local coordinates");
        auto sheared = hierarchy;
        sheared.nodes[1].transform.scale = {2, 1, 3};
        sheared.instances[0].transform.rotationDegrees[1] = 45;
        Manifest affineWorld;
        Check(ResolveHierarchy(sheared, affineWorld, error), "Resolve nonuniform parent scale and rotated child");
        HitTriangleMesh vertexMesh;
        vertexMesh.vertices.push_back({1, 0, 0});
        HitTriangleMesh transformed;
        Check(TransformHitMesh(vertexMesh, affineWorld.instances[0].transform, transformed, error) &&
            std::abs(transformed.vertices[0][0] - (19.0f - 3.0f / std::sqrt(2.0f))) < 0.001f &&
            std::abs(transformed.vertices[0][2] + std::sqrt(2.0f)) < 0.001f,
            "Collision uses exact parent-child affine composition, including shear");
        Check(!SetNodeParent(loaded, "group", "enemy/tank", error) &&
            !SetNodeParent(loaded, "root", "group", error) &&
            !SetNodeParent(loaded, "mesh/tile", "missing", error),
            "Reject cycles, root parenting and missing parents without changing the map");
        loaded.nodes[0].parentId = "group";
        Check(!SerializeManifest(loaded, roundTrip, error), "Reject a parented Root during save");
        Check(SerializeManifest({}, roundTrip, error), "Prepare legacy map");
        auto legacyTree = Json::parse(roundTrip);
        legacyTree["version"] = 1;
        legacyTree.erase("nodes");
        Check(DeserializeManifest(legacyTree.dump(), loaded, error) && loaded.nodes[0].id == "root",
            "Legacy maps acquire an identity Root");
        Manifest ordered;
        ordered.nodes.push_back({"a", "A", "root", {}});
        ordered.nodes.push_back({"b", "B", "root", {}});
        ordered.enemies.push_back({"tank", "Tank", {}, {}, "root"});
        const auto children = [](const Manifest& map, const std::string& parent)
        {
            std::vector<std::string> ids;
            for (const auto& n : MapTree(map)) if (n.parentId == parent) ids.push_back(n.id);
            return ids;
        };
        Check(MoveNode(ordered, "enemy/tank", "a", NodePlacement::Before, error) &&
            children(ordered, "root") == std::vector<std::string>{"enemy/tank", "a", "b", "player-start"},
            "Insert enemy before an Empty sibling across payload types");
        Check(MoveNode(ordered, "enemy/tank", "b", NodePlacement::After, error) &&
            children(ordered, "root") == std::vector<std::string>{"a", "b", "enemy/tank", "player-start"},
            "Insert after target without changing parent");
        Check(MoveNode(ordered, "a", "b", NodePlacement::Child, error) &&
            MoveNode(ordered, "enemy/tank", "b", NodePlacement::Child, error) &&
            children(ordered, "b") == std::vector<std::string>{"a", "enemy/tank"},
            "Center drop appends children in delivery order");
        Check(MoveNode(ordered, "enemy/tank", "a", NodePlacement::Before, error) &&
            children(ordered, "b") == std::vector<std::string>{"enemy/tank", "a"},
            "Reorder within the same parent without an index shift error");
        Check(SerializeManifest(ordered, roundTrip, error) && DeserializeManifest(roundTrip, loaded, error) &&
            children(loaded, "b") == children(ordered, "b"), "Sibling order survives save and reload");
        std::string beforeMove;
        Check(SerializeManifest(ordered, beforeMove, error), "Snapshot tree before rejected moves");
        Check(!MoveNode(ordered, "root", "a", NodePlacement::Child, error) &&
            !MoveNode(ordered, "a", "root", NodePlacement::Before, error) &&
            !MoveNode(ordered, "a", "root", NodePlacement::After, error) &&
            !MoveNode(ordered, "b", "a", NodePlacement::Child, error) &&
            !MoveNode(ordered, "b", "enemy/tank", NodePlacement::Before, error) &&
            !MoveNode(ordered, "b", "enemy/tank", NodePlacement::After, error) &&
            !MoveNode(ordered, "missing", "a", NodePlacement::Child, error) &&
            !MoveNode(ordered, "a", "missing", NodePlacement::After, error) &&
            !MoveNode(ordered, "a", "a", NodePlacement::After, error),
            "Reject root moves, root siblings, cycles and self drops in all zones");
        Check(SerializeManifest(ordered, roundTrip, error) && roundTrip == beforeMove,
            "Rejected drops preserve hierarchy and sibling order");
        Check(MoveNode(ordered, "a", "root", NodePlacement::Child, error), "Root accepts center drops");
        Check(MoveNode(ordered, "b", "player-start", NodePlacement::After, error) &&
            children(ordered, "b") == std::vector<std::string>{"enemy/tank"},
            "Moving a parent preserves its child subtree");
        using Tank::Ui::SceneTreeDropPosition;
        const std::vector<Tank::Ui::SceneTreeItem> uiItems = {
            {"root", "", "Root"}, {"group", "root", "Group"}, {"child", "group", "Child"}};
        Check(!Tank::Ui::SceneTreeDropAllowed(uiItems, "root", {"root", "group", SceneTreeDropPosition::Child}) &&
            !Tank::Ui::SceneTreeDropAllowed(uiItems, "root", {"group", "child", SceneTreeDropPosition::Child}) &&
            !Tank::Ui::SceneTreeDropAllowed(uiItems, "root", {"group", "child", SceneTreeDropPosition::After}) &&
            !Tank::Ui::SceneTreeDropAllowed(uiItems, "root", {"child", "root", SceneTreeDropPosition::Before}) &&
            Tank::Ui::SceneTreeDropAllowed(uiItems, "root", {"child", "root", SceneTreeDropPosition::Child}),
            "Shared SceneTreeUI independently enforces Root and cycle restrictions");
        Check(Tank::Ui::SceneTreeDropZone(2, 20) == SceneTreeDropPosition::Before &&
            Tank::Ui::SceneTreeDropZone(10, 20) == SceneTreeDropPosition::Child &&
            Tank::Ui::SceneTreeDropZone(18, 20) == SceneTreeDropPosition::After,
            "Row top, center and bottom map to the advertised drop operations");
        Manifest basics;
        basics.enemies.push_back({"tank", "Tank", {3, 1, 7}, {0, 90, 0}});
        const auto copiedEnemy = DuplicateNode(basics, "enemy/tank", error);
        Check(copiedEnemy && basics.enemies.size() == 2 && basics.enemies[1].position == basics.enemies[0].position,
            "Tree duplicates enemies with a fresh ID and the same local pose");
        const auto copiedStart = DuplicateNode(basics, "player-start", error);
        Check(copiedStart && basics.startPoints.size() == 1 && !GetNodeInfo(basics, *copiedStart).active &&
            GetNodeInfo(basics, "player-start").active, "Duplicated StartPoint starts inactive");
        Transform startPose; startPose.position = {20, 3, 40};
        Check(SetNodeLocalTransform(basics, *copiedStart, startPose) && SetNodeActive(basics, *copiedStart, true, error) &&
            !GetNodeInfo(basics, "player-start").active && GetNodeInfo(basics, *copiedStart).active,
            "Activating a StartPoint atomically deactivates the previous one");
        Check(!SetNodeActive(basics, *copiedStart, false, error) &&
            GetNodeInfo(basics, *copiedStart).active, "Cannot deactivate the only active StartPoint");
        auto invalidStarts = basics;
        invalidStarts.nodeInfo["player-start"].active = true;
        Check(!SerializeManifest(invalidStarts, roundTrip, error), "Reject multiple active StartPoints on save");
        invalidStarts = basics;
        invalidStarts.nodeInfo[*copiedStart].active = false;
        Check(!SerializeManifest(invalidStarts, roundTrip, error), "Reject maps without an active StartPoint");
        Check(!SetNodeActive(basics, "root", false, error), "Cannot deactivate ancestors of the active StartPoint");
        Check(ResolveHierarchy(basics, world, error) && world.playerSpawn.position == startPose.position &&
            world.startPoints.empty(), "Gameplay uses exactly the active StartPoint");
        Check(SetNodeActive(basics, "enemy/tank", false, error) && ResolveHierarchy(basics, world, error) &&
            world.enemies.size() == 1, "Inactive enemies are excluded from gameplay");
        Check(ResolveHierarchy(basics, world, error, true) && world.enemies.size() == 2,
            "Inactive nodes remain available in the editor");
        auto enemyInfo = GetNodeInfo(basics, *copiedEnemy);
        enemyInfo.visible = false;
        basics.nodeInfo[*copiedEnemy] = enemyInfo;
        Check(ResolveHierarchy(basics, world, error, true) && world.enemies.size() == 1 &&
            ResolveHierarchy(basics, world, error) && world.enemies.size() == 1,
            "Editor visibility is independent of gameplay activation");
        Check(SerializeManifest(basics, roundTrip, error) && DeserializeManifest(roundTrip, loaded, error) &&
            loaded.startPoints.size() == 1 && !GetNodeInfo(loaded, *copiedEnemy).visible &&
            GetNodeInfo(loaded, *copiedStart).active, "Persist common node information and all StartPoints");
        Check(RemoveNode(loaded, "player-start", error) && loaded.startPoints.empty() &&
            GetNodeInfo(loaded, "player-start").active && loaded.playerSpawn.position == startPose.position,
            "Removing the original StartPoint preserves the remaining active start");
        Check(!RemoveNode(loaded, "player-start", error) && !RemoveNode(loaded, "root", error) &&
            !DuplicateNode(loaded, "root", error), "Protect the last StartPoint and Root");
        basics.nodes.push_back({"group", "Group", "root", {}});
        Check(SetNodeParent(basics, "enemy/tank", "group", error), "Parent enemy for inherited flags");
        basics.nodeInfo["group"] = {false, false};
        Check(!NodeIsEnabled(basics, "enemy/tank") && !NodeIsEnabled(basics, "enemy/tank", true),
            "Active and Visible are inherited from ancestors");
        basics.instances.push_back({"mesh", "Models/tile.gltf", {}, "group"});
        Check(MoveNode(basics, "enemy/tank", "mesh/mesh", NodePlacement::Before, error), "Order mixed subtree children");
        const auto copiedGroup = DuplicateNode(basics, "group", error);
        Check(copiedGroup && children(basics, *copiedGroup).size() == 2 &&
            !GetNodeInfo(basics, *copiedGroup).visible, "Duplicate subtrees and their common node information");
        const auto clonedChildren = children(basics, *copiedGroup);
        Check(clonedChildren[0].starts_with("enemy/") && clonedChildren[1].starts_with("mesh/"),
            "Subtree duplication preserves sibling order across different node kinds");
        Check(!RemoveNode(basics, "group", error), "Do not orphan children on removal");
        Manifest names;
        names.nodes.push_back({"group", "Group", "root", {}});
        names.instances.push_back({"tile", "Models/tile.gltf", {}, "group"});
        names.enemies.push_back({"tank", "Fixed turret", {}, {}});
        names.clearAreas.push_back({"zone", "Zone", {}, {1, 1, 1}});
        names.startPoints.push_back({"other", {}, "root"});
        const std::vector<std::string> renameIds = {"root", "group", "mesh/tile", "enemy/tank",
            "player-start", "start/other", "area/zone"};
        for (const auto& id : renameIds)
            Check(RenameNode(names, id, "Named " + id, error), "All Tree element kinds support Rename");
        Check(!RenameNode(names, "mesh/tile", " \t", error) &&
            !RenameNode(names, "missing", "Missing", error), "Reject empty names and missing nodes");
        std::string renamedJson;
        Check(SerializeManifest(names, renamedJson, error) && DeserializeManifest(renamedJson, loaded, error),
            "Persist custom Tree names");
        for (const auto& node : MapTree(loaded))
            Check(node.name == "Named " + node.id, "Renamed display names survive save/load");
        Check(loaded.instances[0].id == "tile" && loaded.instances[0].parentId == "group" &&
            loaded.instances[0].asset == "Models/tile.gltf" && loaded.enemies[0].unitType == "Fixed turret",
            "Rename preserves IDs, hierarchy, asset and enemy type references");
        const auto renamedCopy = DuplicateNode(loaded, "group", error);
        Check(renamedCopy && GetNodeInfo(loaded, *renamedCopy).name == "Named group Copy",
            "Duplicate gives a custom named subtree a Copy suffix");
        Check(RemoveNode(loaded, "mesh/tile", error) && !loaded.nodeInfo.contains("mesh/tile"),
            "Delete cleans up custom display names");
        names.instances[0].breakable = true;
        names.instances[0].hitPoints = 125;
        Check(SerializeManifest(names, renamedJson, error) && DeserializeManifest(renamedJson, loaded, error) &&
            loaded.instances[0].breakable && loaded.instances[0].hitPoints == 125,
            "Persist mesh breakability and HP");
        const auto breakableCopy = DuplicateNode(loaded, "mesh/tile", error);
        Check(breakableCopy.has_value(), "Duplicate breakable mesh");
        bool copiedHP = false;
        for (const auto& model : loaded.instances)
            if ("mesh/" + model.id == *breakableCopy) copiedHP = model.breakable && model.hitPoints == 125;
        Check(copiedHP, "Duplicate preserves Breakable and HP");
        names.instances[0].hitPoints = 0;
        Check(!SerializeManifest(names, renamedJson, error), "Reject nonpositive mesh HP");
        Manifest supported;
        supported.instances.push_back({"platform", "Models/box.gltf", {{0,0,12}, {}}, "root", true, 20});
        supported.nodes.push_back({"adapter", "Adapter", "mesh/platform", {{0,3,0}, {}}});
        supported.enemies.push_back({"turret", "Fixed turret", {0,0.5f,0}, {}, "adapter", true});
        Check(SerializeManifest(supported, renamedJson, error) && DeserializeManifest(renamedJson, loaded, error) &&
            loaded.enemies[0].supportedByParent, "Persist Supported by parent");
        Manifest supportWorld;
        Check(ResolveHierarchy(loaded, supportWorld, error) && supportWorld.enemies[0].supportPlacementId == "platform" &&
            supportWorld.enemies[0].position[1] == 3.5f && supportWorld.enemies[0].parentId == "root",
            "Resolve nearest Breakable ancestor before flattening world poses");
        loaded.enemies[0].supportedByParent = false;
        Check(ResolveHierarchy(loaded, supportWorld, error) && supportWorld.enemies[0].supportPlacementId.empty(),
            "Opt out of parent support");
        Manifest areas;
        areas.clearAreas.push_back({"box", "Box", {3,4,5}, {2,3,4}, "root", {10,20,30}});
        Check(SerializeManifest(areas, renamedJson, error) && DeserializeManifest(renamedJson, loaded, error) &&
            loaded.clearAreas[0].rotationDegrees == std::array<float,3>{10,20,30} &&
            loaded.clearAreas[0].size == std::array<float,3>{2,3,4}, "Clear Area saves common position/rotation/scale");
        const auto areaTransform = NodeLocalTransform(loaded, "area/box");
        Check(areaTransform.rotationDegrees[1] == 20 && areaTransform.scale[2] == 4, "Tree transform retains Clear Area rotation and scale");
        auto legacyArea = nlohmann::json::parse(renamedJson);
        auto& entry = legacyArea["clearAreas"][0];
        entry["center"] = entry["position"]; entry["size"] = entry["scale"];
        entry.erase("position"); entry.erase("rotationDegrees"); entry.erase("scale");
        Check(DeserializeManifest(legacyArea.dump(), loaded, error) && loaded.clearAreas[0].center[0] == 3 &&
            loaded.clearAreas[0].size[2] == 4 && loaded.clearAreas[0].rotationDegrees[1] == 0,
            "Legacy Center/Size regions preserve dimensions with zero rotation");
        std::cout << "PASS MapManifest\n";
        return 0;
    }
    catch (const std::exception& exception)
    {
        std::cerr << "FAIL MapManifest: " << exception.what() << '\n';
        return 1;
    }
}
