#pragma once

#include "PhysicsTypes.h"
#include <array>
#include <cstdint>
#include <span>
#include <optional>
#include <string>
#include <vector>

namespace Tank::Physics
{
    enum class NavigationTerrain { Road, Ground, Rough, Marsh, Count };
    inline constexpr size_t kNavigationTerrainCount = static_cast<size_t>(NavigationTerrain::Count);

    // Independent of player/AI ownership; intended for the shared tank definition.
    // No gameplay defaults: the caller supplies tuned values (or loads them from JSON).
    struct NavigationMobility
    {
        float speedMetersPerSecond = 0;
        float widthMeters = 0;
        float heightMeters = 0;
        float maximumSlopeDegrees = 0;
        float maximumWaterDepthMeters = 0;
        float maximumStepMeters = 0;
        std::array<float, kNavigationTerrainCount> terrainCosts = {};
    };

    struct NavigationCell
    {
        int x = 0;
        int z = 0;
        std::string surfaceId;
        Vec3 position;
        NavigationTerrain terrain = NavigationTerrain::Ground;
        float clearanceWidthMeters = 0;
        float clearanceHeightMeters = 0;
        float slopeDegrees = 0;
        float waterDepthMeters = 0;
        bool passable = true;
        std::optional<float> supportHeightMeters; // Ground height at cell.position XZ, distinct from vehicle-center Y.
        std::array<float, 2> supportGradientXZ = {}; // Rise per meter along world X and Z.
    };

    struct NavigationPassage
    {
        float clearanceWidthMeters = 0;
        float clearanceHeightMeters = 0;
        float stepMeters = 0;
    };

    // Directed explicit links. There is no implicit connection between stacked cells.
    struct NavigationLink
    {
        size_t from = 0;
        size_t to = 0;
        NavigationPassage passage;
    };

    struct NavigationGrid
    {
        std::vector<NavigationCell> cells;
        std::vector<NavigationLink> links;
        std::uint64_t revision = 0;
    };

    struct NavigationPath
    {
        std::vector<NavigationCell> cells;
        std::vector<NavigationPassage> passages;
        double estimatedSeconds = 0;
        std::uint64_t gridRevision = 0;
    };

    bool IsValidNavigationMobility(const NavigationMobility& mobility);
    bool CanTraverseNavigationCell(const NavigationCell& cell, const NavigationMobility& mobility);
    bool CanTraverseNavigationPassage(const NavigationCell& from, const NavigationCell& to,
        const NavigationPassage& passage, const NavigationMobility& mobility);
    double NavigationDistance(const Vec3& from, const Vec3& to);
    // Each edge uses the source terrain for its first half and destination terrain for its second half.
    // Group cost is the slowest member's time per meter in each half. Pass only moving members.
    bool FindNavigationPath(const NavigationGrid& grid, size_t start, size_t goal,
        std::span<const NavigationMobility> movingMembers, NavigationPath& path, std::string& error);

    bool SerializeNavigationGrid(const NavigationGrid& grid, std::string& text, std::string& error);
    bool DeserializeNavigationGrid(const std::string& text, NavigationGrid& grid, std::string& error);
    bool ValidateNavigationGrid(const NavigationGrid& grid, std::string& error);
    bool SerializeNavigationMobility(const NavigationMobility& mobility, std::string& text, std::string& error);
    bool DeserializeNavigationMobility(const std::string& text, NavigationMobility& mobility, std::string& error);
}
