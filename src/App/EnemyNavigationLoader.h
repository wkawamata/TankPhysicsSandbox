#pragma once
#include "Map/MapManifest.h"
#include "Physics/EnemyNavigationSetup.h"
#include "Physics/EnemyNavigationPlanner.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>

namespace Tank::App
{
    inline bool ValidateEnemyNavigationSetup(const Physics::EnemyNavigationSetup& setup,
        const Map::Manifest& manifest, std::string& error)
    {
        if (!Physics::ValidateNavigationGrid(setup.grid, error)) return false;
        for (const auto& driver : setup.drivers)
        {
            const auto group = std::find_if(manifest.enemyGroups.begin(), manifest.enemyGroups.end(),
                [&](const auto& value) { return value.id == driver.groupId; });
            const auto enemy = std::find_if(manifest.enemies.begin(), manifest.enemies.end(),
                [&](const auto& value) { return value.id == driver.enemyId; });
            if (group == manifest.enemyGroups.end() || !group->patrol || enemy == manifest.enemies.end() ||
                std::none_of(group->members.begin(), group->members.end(),
                    [&](const auto& member) { return member.enemyId == driver.enemyId; }))
            { error = "Navigation driver must belong to an existing patrol group"; return false; }
            if (group->patrol->points.empty() || driver.snapDistanceMeters > group->patrol->arrivalMeters)
            { error = "Navigation snap distance exceeds patrol arrival radius"; return false; }
            Physics::EnemyNavigationPlanner planner;
            if (!planner.Configure(setup.grid, std::span(&driver.mobility, 1), driver.snapDistanceMeters))
            { error = "Invalid navigation grid or mobility"; return false; }
            auto anchor = Physics::Vec3{enemy->position[0], enemy->position[1], enemy->position[2]};
            auto surface = driver.surfaceId;
            for (size_t i = 0; i <= group->patrol->points.size(); ++i)
            {
                const auto& point = group->patrol->points[i % group->patrol->points.size()];
                Physics::EnemyGroupBehaviorSnapshot request;
                request.hasMoveTarget = true;
                request.moveTarget = {point.position[0], point.position[1], point.position[2]};
                request.targetSurfaceId = point.surfaceId;
                planner.Update(request, anchor, surface);
                if (planner.State().stopped)
                { error = "Unreachable patrol point for " + driver.enemyId + ": " + planner.State().error; return false; }
                anchor = request.moveTarget;
                surface = point.surfaceId;
            }
        }
        error.clear();
        return true;
    }

    // Optional map-local file; absence disables navigation without changing old maps.
    inline bool LoadEnemyNavigationSetup(const std::filesystem::path& folder, const Map::Manifest& manifest,
        std::optional<Physics::EnemyNavigationSetup>& setup, std::string& error)
    {
        const auto path = folder / "navigation.json";
        std::error_code filesystemError;
        const bool exists = std::filesystem::exists(path, filesystemError);
        if (filesystemError) { error = filesystemError.message(); return false; }
        if (!exists) { setup.reset(); error.clear(); return true; }
        std::ifstream input(path, std::ios::binary);
        if (!input) { error = "Cannot open " + path.string(); return false; }
        const std::string text((std::istreambuf_iterator<char>(input)), {});
        if (input.bad()) { error = "Cannot read " + path.string(); return false; }
        Physics::EnemyNavigationSetup candidate;
        if (!Physics::DeserializeEnemyNavigationSetup(text, candidate, error) ||
            !ValidateEnemyNavigationSetup(candidate, manifest, error)) return false;
        setup = std::move(candidate);
        error.clear();
        return true;
    }
}
