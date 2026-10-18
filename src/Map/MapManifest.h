#pragma once

#include <array>
#include <string>
#include <optional>
#include <vector>

namespace Tank::Map
{
    struct Transform
    {
        std::array<float, 3> position = { 0.0f, 0.0f, 0.0f };
        std::array<float, 3> rotationDegrees = { 0.0f, 0.0f, 0.0f };
        // Per-axis scale keeps Visual and HitMesh placement in the same meter space.
        std::array<float, 3> scale = { 1.0f, 1.0f, 1.0f };
    };

    struct Instance
    {
        std::string id;
        // UTF-8 path relative to the map folder, using forward slashes.
        std::string asset;
        Transform transform;
    };

    struct EnemyPlacement
    {
        std::string id;
        // Name in the enemy unit catalog; resolved when loading gameplay.
        std::string unitType;
        std::array<float, 3> position = { 0.0f, 0.0f, 0.0f };
        std::array<float, 3> rotationDegrees = { 0.0f, 0.0f, 0.0f };
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
        // Full extents in meters, along world axes.
        std::array<float, 3> size = { 1.0f, 1.0f, 1.0f };
    };

    struct Manifest
    {
        Transform playerSpawn = { { 0.0f, 2.0f, 0.0f }, {} };
        std::vector<Instance> instances;
        std::vector<ClearArea> clearAreas;
        std::vector<EnemyPlacement> enemies;
        std::vector<EnemyGroup> enemyGroups;
    };

    // Failed operations leave the output unchanged and provide an error.
    bool SerializeManifest(const Manifest& manifest, std::string& jsonText, std::string& error);
    bool DeserializeManifest(const std::string& jsonText, Manifest& manifest, std::string& error);
}
