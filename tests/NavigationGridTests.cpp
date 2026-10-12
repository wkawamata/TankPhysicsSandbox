#include "Physics/NavigationGrid.h"
#include "Physics/EnemyNavigationPlanner.h"
#include "Physics/SimpleNavigationMovement.h"
#include <nlohmann/json.hpp>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

int main()
{
    using namespace Tank::Physics;
    try
    {
        const auto check = [](bool valid, const char* message)
            { if (!valid) throw std::runtime_error(message); };
        NavigationMobility tank{2, 2, 2, 30, 1, 0.5f, {1, 1.25f, 2, 10}};
        const auto cell = [](int x, int z, NavigationTerrain terrain)
        {
            return NavigationCell{x, z, "ground", {float(x), 0, float(z)}, terrain, 10, 10, 0, 0, true};
        };
        NavigationGrid grid;
        grid.cells = {cell(0, 0, NavigationTerrain::Road), cell(1, 0, NavigationTerrain::Marsh),
            cell(2, 0, NavigationTerrain::Road), cell(0, 1, NavigationTerrain::Road),
            cell(1, 1, NavigationTerrain::Road), cell(2, 1, NavigationTerrain::Road)};
        const auto join = [&](size_t a, size_t b)
        {
            grid.links.push_back({a, b, {10, 10, 0}});
            grid.links.push_back({b, a, {10, 10, 0}});
        };
        join(0, 1); join(1, 2); join(0, 3); join(3, 4); join(4, 5); join(5, 2);
        grid.revision = 42;
        NavigationPath road;
        std::string error;
        std::vector<NavigationMobility> group{tank};
        check(FindNavigationPath(grid, 0, 2, group, road, error) && road.cells.size() == 5 && road.gridRevision == 42 &&
            std::abs(road.estimatedSeconds - 2) < 1e-9, "Longer paved route beats short marsh route");
        SimpleNavigationMovement movement;
        check(movement.Initialize(road, tank) && movement.Advance(road.estimatedSeconds) && movement.State().arrived &&
            movement.State().position.x == 2 && movement.State().speedMetersPerSecond == 0,
            "Estimated single-vehicle time matches actual simple traversal");
        auto slow = tank;
        slow.speedMetersPerSecond = 1;
        slow.widthMeters = 3;
        group.push_back(slow);
        NavigationPath groupPath;
        check(FindNavigationPath(grid, 0, 2, group, groupPath, error) && groupPath.estimatedSeconds == 4,
            "Shared route time accounts for slowest moving member");
        grid.cells[4].clearanceWidthMeters = 2.5f;
        check(FindNavigationPath(grid, 0, 2, std::span(&tank, 1), road, error) && road.cells.size() == 5,
            "Narrow route still suits smaller tank individually");
        NavigationPath marsh;
        check(FindNavigationPath(grid, 0, 2, group, marsh, error) && marsh.cells.size() == 3 &&
            marsh.estimatedSeconds == 11, "Narrow detour is excluded for all group members");
        SimpleNavigationMovement whole, split;
        check(whole.Initialize(marsh, tank) && split.Initialize(marsh, tank), "Initialize shared path per vehicle");
        whole.Advance(3);
        for (int i = 0; i < 300; ++i) split.Advance(0.01);
        check(std::abs(whole.State().position.x - 1.05f) < 1e-5f &&
            std::abs(whole.State().position.x - split.State().position.x) < 1e-5f,
            "Large and small updates charge all crossed terrain intervals");
        const auto preserved = whole.State();
        check(!whole.Advance(-1) && !whole.Advance((std::numeric_limits<double>::infinity)()) &&
            whole.State().position.x == preserved.position.x, "Invalid update preserves movement");
        whole.Advance(100);
        check(whole.State().arrived && whole.State().position.x == 2, "Large update clamps at endpoint");
        grid.cells[1].waterDepthMeters = 2;
        auto oldPath = marsh;
        check(!FindNavigationPath(grid, 0, 2, group, marsh, error) && marsh.estimatedSeconds == oldPath.estimatedSeconds,
            "Water limit blocks remaining route and failed search preserves output");
        grid.cells[1].waterDepthMeters = 0;
        grid.links[0].passage.clearanceHeightMeters = 1;
        check(!FindNavigationPath(grid, 0, 2, group, marsh, error), "Passage headroom blocks route");
        grid.links[0].passage.clearanceHeightMeters = 10;
        grid.links[0].passage.stepMeters = 1;
        check(!FindNavigationPath(grid, 0, 2, group, marsh, error), "Passage step exceeds capability");
        grid.links[0].passage.stepMeters = 0;
        grid.cells[1].slopeDegrees = 31;
        check(!FindNavigationPath(grid, 0, 2, group, marsh, error), "Cell slope exceeds capability");
        grid.cells[1].slopeDegrees = 0;
        grid.cells[1].passable = false;
        check(!FindNavigationPath(grid, 0, 2, group, marsh, error), "Explicit impassable flag blocks route");

        NavigationGrid stacked;
        stacked.cells = {cell(0, 0, NavigationTerrain::Road), cell(0, 0, NavigationTerrain::Road),
            cell(1, 0, NavigationTerrain::Road)};
        stacked.cells[1].surfaceId = "bridge";
        stacked.cells[1].position.y = 0.5f;
        stacked.cells[2].surfaceId = "bridge";
        stacked.cells[2].position.y = 0.5f;
        stacked.links = {{1, 2, {10, 10, 0}}, {2, 1, {10, 10, 0}}};
        check(!FindNavigationPath(stacked, 0, 1, std::span(&tank, 1), marsh, error),
            "Coincident ground and bridge cells have no implicit connection");
        stacked.links.push_back({0, 2, {10, 10, 0}});
        check(FindNavigationPath(stacked, 0, 1, std::span(&tank, 1), marsh, error) &&
            marsh.cells.size() == 3, "Explicit ramp connects correct bridge layer");
        check(movement.Initialize(marsh, tank), "Initialize ramp path");
        movement.Advance(100);
        check(movement.State().surfaceId == "bridge" && movement.State().position.y == 0.5f,
            "Simple movement preserves bridge identity and elevation");
        auto flatOnly = tank;
        flatOnly.maximumSlopeDegrees = 0;
        check(!FindNavigationPath(stacked, 0, 1, std::span(&flatOnly, 1), marsh, error),
            "Link gradient is checked even when cell slope is zero");
        check(FindNavigationPath(stacked, 1, 1, std::span(&tank, 1), marsh, error) &&
            marsh.cells.size() == 1 && marsh.estimatedSeconds == 0 && movement.Initialize(marsh, tank) &&
            movement.State().arrived, "Start equals goal has zero-time stationary path");
        stacked.cells.push_back(stacked.cells[0]);
        check(!FindNavigationPath(stacked, 0, 1, std::span(&tank, 1), marsh, error), "Duplicate grid cell rejected");
        check(!FindNavigationPath(grid, 0, 2, {}, marsh, error), "Empty moving group rejected");

        std::string json;
        check(SerializeNavigationMobility(tank, json, error), "Save mobility JSON");
        NavigationMobility loaded;
        check(DeserializeNavigationMobility(json, loaded, error) && loaded.terrainCosts == tank.terrainCosts &&
            loaded.maximumStepMeters == tank.maximumStepMeters, "All tuning survives JSON round trip");
        auto invalid = nlohmann::json::parse(json);
        invalid["terrainCosts"]["marsh"] = 0;
        check(!DeserializeNavigationMobility(invalid.dump(), loaded, error) && loaded.terrainCosts == tank.terrainCosts,
            "Invalid terrain cost preserves previous settings");
        invalid = nlohmann::json::parse(json);
        invalid["speedMetersPerSecond"] = 1e100;
        check(!DeserializeNavigationMobility(invalid.dump(), loaded, error), "Out-of-range JSON rejected");
        auto badMobility = tank;
        badMobility.widthMeters = -1;
        const auto before = json;
        check(!SerializeNavigationMobility(badMobility, json, error) && json == before,
            "Invalid serialization preserves output");
        check(!movement.Initialize(oldPath, badMobility) && movement.State().arrived,
            "Invalid mobility does not replace initialized movement");
        std::string gridJson;
        check(SerializeNavigationGrid(grid, gridJson, error), "Serialize map walkable surfaces");
        NavigationGrid authored;
        check(DeserializeNavigationGrid(gridJson, authored, error) && authored.cells.size() == 6 &&
            authored.links.size() == grid.links.size(), "Load authored navigation grid");
        auto invalidGrid = nlohmann::json::parse(gridJson);
        invalidGrid["links"][0]["to"] = -1;
        check(!DeserializeNavigationGrid(invalidGrid.dump(), authored, error) && authored.cells.size() == 6,
            "Invalid navigation file preserves previous map");
        // Reopen the original paved/marsh map with the detour available.
        authored.cells[1].passable = true;
        authored.cells[4].clearanceWidthMeters = 10;
        EnemyNavigationPlanner planner;
        check(planner.Configure(authored, std::span(&tank, 1), 0.25), "Configure authored group navigation");
        EnemyGroupBehaviorSnapshot request;
        request.hasMoveTarget = true;
        request.moveTarget = {2, 0, 0};
        request.targetSurfaceId = "ground";
        planner.Update(request, {}, "ground");
        check(!planner.State().stopped && planner.State().path && planner.State().path->cells.size() == 5,
            "Behavior movement target resolves to authored route");
        planner.Update(request, {}, "ground");
        check(planner.State().searches == 1, "Unchanged start and target reuse group route");
        SimpleNavigationMovement follower;
        check(follower.Initialize(*planner.State().path, tank) && follower.Advance(2) && follower.State().arrived,
            "Authored map and behavior target drive simple movement end to end");
        request.targetSurfaceId = "missing";
        planner.Update(request, {}, "ground");
        check(planner.State().stopped && !planner.State().path && !planner.State().error.empty(),
            "Unknown surface stops instead of using another layer or stale route");
        request.targetSurfaceId.clear();
        planner.Update(request, {}, "ground");
        check(planner.State().stopped, "Unknown observed target layer is not guessed");
        request.hasMoveTarget = false;
        planner.Update(request, {}, "ground");
        check(planner.State().stopped && planner.State().error.empty(), "Behavior stop clears route cleanly");
        NavigationGrid layers;
        layers.cells = {{0, 0, "ground", {0, 1.5f, 0}, NavigationTerrain::Road, 10, 10, 0, 0, true, 0.0f},
            {0, 0, "bridge", {0, 5.5f, 0}, NavigationTerrain::Road, 10, 10, 0, 0, true, 4.0f}};
        EnemyNavigationPlanner observer;
        check(observer.Configure(layers, std::span(&tank, 1), 0.25), "Configure stacked observations");
        auto observed = observer.ResolveObservation({0, 0.84f, 0}, 4, 6, 0.2f);
        check(observed && observed->surfaceId == "bridge" && observed->position.y == 5.5f,
            "Physical support chooses bridge independent of vehicle-center height");
        observed = observer.ResolveObservation({0, 5.5f, 0}, 0, 6, 0.2f);
        check(observed && observed->surfaceId == "ground", "Ground support chooses lower road");
        check(!observer.ResolveObservation({100, 1, 0}, 0, 6, 0.2f) &&
            !observer.ResolveObservation({}, 2, 6, 0.2f), "Unmapped positions and support heights remain unresolved");
        request.hasMoveTarget = true;
        request.moveTarget = layers.cells[1].position;
        request.targetSurfaceId = "bridge";
        observer.Update(request, layers.cells[0].position, "ground");
        check(observer.State().stopped, "Bridge observation cannot invent a cross-layer connection");
        layers.cells[1].supportHeightMeters = 0.0f;
        check(observer.Configure(layers, std::span(&tank, 1), 0.25) &&
            !observer.ResolveObservation({}, 0, 6, 0.2f), "Ambiguous coincident layers stop rather than guess");
        check(SerializeNavigationGrid(layers, gridJson, error) && DeserializeNavigationGrid(gridJson, authored, error) &&
            authored.cells[1].supportHeightMeters == 0, "Authored support heights survive JSON round trip");
        auto brokenHeight = nlohmann::json::parse(gridJson);
        brokenHeight["cells"][0]["supportHeightMeters"] = "unknown";
        check(!DeserializeNavigationGrid(brokenHeight.dump(), authored, error) && authored.cells.size() == 2,
            "Invalid support height preserves prior navigation map");
        layers.cells[0].supportGradientXZ = {0.25f, -0.5f};
        layers.cells[1].supportHeightMeters = 4.0f;
        layers.cells[1].supportGradientXZ = {0.25f, -0.5f};
        auto limitedSlope = tank;
        limitedSlope.maximumSlopeDegrees = 20;
        check(CanTraverseNavigationCell(layers.cells[0], tank) && !CanTraverseNavigationCell(layers.cells[0], limitedSlope),
            "Support-plane slope is included in vehicle climbing limits");
        check(observer.Configure(layers, std::span(&tank, 1), 0.25), "Configure sloping stacked roads");
        observed = observer.ResolveObservation({2, 1, -2}, 1.5f, 6, 0.05f);
        check(observed && observed->surfaceId == "ground", "Uphill observation uses height at target XZ");
        observed = observer.ResolveObservation({-2, 7, 2}, 2.5f, 6, 0.05f);
        check(observed && observed->surfaceId == "bridge", "Downhill bridge observation preserves upper layer");
        observed = observer.ResolveObservation({0, 1, 0}, Vec3{2, 0.5f, 0}, 6, 0.05f);
        check(observed && observed->surfaceId == "ground", "Offset wheel contact is evaluated at contact XZ, not body center");
        check(!observer.ResolveObservation({2, 1, -2}, 0, 6, 0.05f), "Cell-center height is not used away from slope origin");
        check(!observer.ResolveObservation({20, 1, -20}, 15, 6, 0.05f), "Slope extrapolation respects capture radius");
        check(SerializeNavigationGrid(layers, gridJson, error) && DeserializeNavigationGrid(gridJson, authored, error) &&
            authored.cells[0].supportGradientXZ == layers.cells[0].supportGradientXZ,
            "Signed XZ gradients survive JSON round trip");
        for (const auto badGradient : {nlohmann::json::array({1}), nlohmann::json::array({1, 2, 3}),
            nlohmann::json::array({1, "2"}), nlohmann::json::array({1, 1e100}), nlohmann::json(true)})
        {
            auto malformed = nlohmann::json::parse(gridJson);
            malformed["cells"][0]["supportGradientXZ"] = badGradient;
            check(!DeserializeNavigationGrid(malformed.dump(), authored, error) &&
                authored.cells[0].supportGradientXZ == layers.cells[0].supportGradientXZ,
                "Malformed slope JSON preserves loaded map");
        }
        auto missingBase = layers;
        missingBase.cells[0].supportHeightMeters.reset();
        check(!ValidateNavigationGrid(missingBase, error), "Slope requires an explicit base support height");
        layers.cells[1].supportHeightMeters = 0.0f;
        check(observer.Configure(layers, std::span(&tank, 1), 0.25) &&
            !observer.ResolveObservation({2, 1, -2}, 1.5f, 6, 0.05f), "Overlapping slopes remain ambiguous");
        std::cout << "PASS NavigationGrid\n";
        return 0;
    }
    catch (const std::exception& exception)
    {
        std::cerr << "FAIL NavigationGrid: " << exception.what() << '\n';
        return 1;
    }
}
