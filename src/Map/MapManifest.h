#pragma once

#include <array>
#include <string>
#include <optional>
#include <map>
#include <vector>

namespace Tank::Map
{
    struct Transform
    {
        std::array<float, 3> position = { 0.0f, 0.0f, 0.0f };
        std::array<float, 3> rotationDegrees = { 0.0f, 0.0f, 0.0f };
        // Per-axis scale keeps Visual and HitMesh placement in the same meter space.
        std::array<float, 3> scale = { 1.0f, 1.0f, 1.0f };
        // Runtime-only affine matrix; preserves shear introduced by parent transforms.
        std::optional<std::array<float, 16>> worldMatrix;
    };

    struct GateSettings
    {
        float triggerRadiusMeters = 12.0f;
        float travelMeters = 3.2f;
        float openingSpeedMetersPerSecond = 2.0f;
    };

    struct Instance
    {
        std::string id;
        // UTF-8 path relative to the map folder, using forward slashes.
        std::string asset;
        Transform transform;
        std::string parentId = "root";
        bool breakable = false;
        float hitPoints = 60.0f;
        std::optional<GateSettings> gate;
    };

    struct EnemyPlacement
    {
        std::string id;
        // Name in the enemy unit catalog; resolved when loading gameplay.
        std::string unitType;
        std::array<float, 3> position = { 0.0f, 0.0f, 0.0f };
        std::array<float, 3> rotationDegrees = { 0.0f, 0.0f, 0.0f };
        std::string parentId = "root";
        bool supportedByParent = false;
        // Runtime-only link resolved before the authored hierarchy is flattened.
        std::string supportPlacementId;
    };

    struct EnemyGroupMember
    {
        std::string enemyId;
        // Lower numbers have higher command priority; unique within the group.
        int commandPriority = 0;
    };

    struct EnemyPatrolPoint
    {
        std::array<float, 3> position = {};
        std::string surfaceId;
    };

    struct EnemyPatrolSettings
    {
        std::vector<EnemyPatrolPoint> points;
        float attackStopMeters = 0;
        float resumeApproachMeters = 0;
        float arrivalMeters = 0;
        float searchWaitSeconds = 0;
    };

    bool IsValidEnemyPatrolSettings(const EnemyPatrolSettings& settings);

    struct EnemyGroup
    {
        std::string id;
        std::vector<EnemyGroupMember> members;
        std::optional<EnemyPatrolSettings> patrol;
    };

    struct ClearArea
    {
        std::string id;
        std::string name;
        std::array<float, 3> center = { 0.0f, 0.0f, 0.0f };
        // Scale of a centered 1 x 1 x 1 meter box (legacy member name retained).
        std::array<float, 3> size = { 1.0f, 1.0f, 1.0f };
        std::string parentId = "root";
        std::array<float, 3> rotationDegrees = {0, 0, 0};
        std::optional<std::array<float, 16>> worldMatrix;
    };

    inline Transform ClearAreaTransform(const ClearArea& area)
    { return {area.center, area.rotationDegrees, area.size, area.worldMatrix}; }

    struct EmptyNode
    {
        std::string id;
        std::string name;
        std::string parentId;
        Transform transform;
    };

    struct Manifest
    {
        // Empty only for legacy manifests; MapName falls back to the folder name.
        std::string name;
        bool playable = true;
        Transform playerSpawn = { { 0.0f, 2.0f, 0.0f }, {} };
        std::vector<Instance> instances;
        std::vector<ClearArea> clearAreas;
        std::vector<EnemyPlacement> enemies;
        std::vector<EnemyGroup> enemyGroups;
        std::vector<EmptyNode> nodes = { { "root", "Root", "", {} } };
        std::string playerSpawnParentId = "root";
        // Stable sibling display order across all node kinds.
        std::vector<std::string> treeOrder;
        struct NodeInfo { bool active = true; bool visible = true; std::string name; };
        std::map<std::string, NodeInfo> nodeInfo;
        struct StartPoint
        {
            std::string id;
            Transform transform;
            std::string parentId = "root";
        };
        // playerSpawn remains the original start node for legacy maps.
        std::vector<StartPoint> startPoints;
    };

    bool IsValidMapName(const std::string& name, std::string& error);

    // Failed operations leave the output unchanged and provide an error.
    bool SerializeManifest(const Manifest& manifest, std::string& jsonText, std::string& error);
    bool DeserializeManifest(const std::string& jsonText, Manifest& manifest, std::string& error);
}
