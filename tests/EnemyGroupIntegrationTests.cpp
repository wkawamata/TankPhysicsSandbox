#include "App/ManifestEnemySpawner.h"
#include "App/EnemyNavigationLoader.h"
#include "Physics/EnemyTrackedSteering.h"

#include <cmath>
#include <iostream>
#include <stdexcept>

int main()
{
    using namespace Tank::Physics;
    try
    {
        const auto check = [](bool valid, const char* message)
            { if (!valid) throw std::runtime_error(message); };
        const auto advance = [](TrackedVehicleTest& world, int count)
            { for (int i = 0; i < count; ++i) world.Step(1.0f / 60); };
        TankState steeringState;
        const auto pivot = EnemyTrackedSteering(steeringState, {10, 0, 0}, 2, 2, 0.5f);
        check(pivot.leftTrack < 0 && pivot.rightTrack > 0 && pivot.throttle > 0 && !pivot.brake,
            "Right turn commands opposing tracks rather than unused steering field");
        const auto behindLeft = EnemyTrackedSteering(steeringState, {-0.01f, 0, -10}, 2, 2, 0.5f, 1);
        const auto behindRight = EnemyTrackedSteering(steeringState, {0.01f, 0, -10}, 2, 2, 0.5f, 1);
        check(behindLeft.leftTrack == behindRight.leftTrack && behindLeft.rightTrack == behindRight.rightTrack,
            "Nearly rearward targets preserve turn direction across angular wrap");
        steeringState.linearVelocity = {0, 0, 2};
        check(EnemyTrackedSteering(steeringState, {10, 0, 0}, 2, 2, 0.5f).brake,
            "Moving vehicle brakes before pivot");
        check(EnemyTrackedSteering(steeringState, {0, 0, 0.7f}, 2, 2, 0.5f).brake,
            "Braking distance limits approach speed");
        steeringState.linearVelocity = {};
        check(EnemyTrackedSteering(steeringState, {0, 0, 0.4f}, 2, 2, 0.5f).brake,
            "Arrival stops the tank");
        TrackedVehicleTest world;
        world.Initialize();
        advance(world, 180);
        EnemyEditorSettings catalog;
        catalog.attackTypes[0].detectionRangeMeters = 1;
        catalog.attackTypes[0].reachMeters = 1;
        catalog.attackTypes[0].firingRangeMeters = 1;
        catalog.attackTypes[0].firingIntervalSeconds = 3600;
        auto observer = EnemyAttackType{};
        observer.name = "Observer";
        observer.firingIntervalSeconds = 3600;
        catalog.attackTypes.push_back(observer);
        EnemyUnitType observerUnit;
        observerUnit.name = "Observer turret";
        observerUnit.attackMounts = {{{}, 1}};
        catalog.unitTypes.push_back(observerUnit);
        Tank::Map::Manifest manifest;
        manifest.enemies = {
            {"leader", "Fixed turret", {0, 1.5f, 12}, {}},
            {"observer", "Observer turret", {8, 1.5f, 12}, {}} };
        manifest.enemyGroups = {{"patrol", {{"observer", 20}, {"leader", 10}}}};
        manifest.enemyGroups[0].patrol = Tank::Map::EnemyPatrolSettings{
            {{{0, 1.5f, 20}, "ground"}}, 5, 7, 1, 3};
        std::string error;
        auto invalid = manifest;
        invalid.enemyGroups[0].members[0].enemyId = "missing";
        check(!Tank::App::SpawnManifestEnemies(world, invalid, catalog, error) &&
            world.State().fixedTurrets.empty() && world.State().enemyGroups.empty(),
            "Invalid groups fail before spawning any bodies");
        check(Tank::App::SpawnManifestEnemies(world, manifest, catalog, error), "Spawn authored group");
        check(world.State().enemyGroups.size() == 1 && world.State().enemyGroups[0].leaderId == "leader",
            "Initial leader elected from authored priority, not member order");
        advance(world, 1);
        check(!world.State().fixedTurrets[0].mounts[0].aim.detected &&
            world.State().fixedTurrets[1].mounts[0].aim.detected && world.State().enemyGroups[0].detected,
            "Nonleader detection is shared without changing leader detection");
        check(world.State().enemyGroups[0].hasLastKnownPosition &&
            std::abs(world.State().enemyGroups[0].lastKnownPosition.z - world.State().bodyPosition.z) < 0.001f,
            "Shared memory records observed player position");
        check(world.State().enemyGroups[0].behavior &&
            world.State().enemyGroups[0].behavior->action == EnemyGroupAction::Approach,
            "Runtime behavior consumes shared nonleader detection");
        for (int shot = 0; shot < 3; ++shot)
        {
            check(world.FireAssault(), "Player shoots leader");
            advance(world, 20);
        }
        check(!world.State().fixedTurrets[0].target.active &&
            world.State().enemyGroups[0].leaderId == "observer" && world.State().enemyGroups[0].detected,
            "Actual destruction transfers command while preserving observation");
        const auto previous = world.State().enemyGroups[0];
        auto badConfig = manifest.enemyGroups;
        badConfig[0].members[0].enemyId = "missing";
        check(!world.ConfigureEnemyGroups(badConfig) && world.State().enemyGroups[0].leaderId == "observer" &&
            world.State().enemyGroups[0].lastObservedSeconds == previous.lastObservedSeconds,
            "Failed runtime configuration preserves leader and memory");
        check(world.AddDestructibleBox({0, 2, 6}, {30, 10, 1}), "Add occluding wall");
        advance(world, 1);
        const auto& hidden = world.State().enemyGroups[0];
        check(!hidden.detected && hidden.hasLastKnownPosition &&
            hidden.lastObservedSeconds == previous.lastObservedSeconds &&
            hidden.lastKnownPosition.x == previous.lastKnownPosition.x &&
            hidden.lastKnownPosition.z == previous.lastKnownPosition.z,
            "Loss of all observers retains last shared observation");
        check(hidden.behavior && hidden.behavior->action == EnemyGroupAction::Search &&
            hidden.behavior->moveTarget.z == previous.lastKnownPosition.z,
            "Runtime loss generates a search target from shared memory");
        check(!world.State().fixedTurrets[1].mounts[0].aim.canFire && world.State().enemyProjectiles.empty(),
            "Shared memory does not authorize firing through occlusion");
        world.Initialize();
        check(world.State().enemyGroups.empty(), "World reset clears groups and observations");
        check(Tank::App::SpawnManifestEnemies(world, manifest, catalog, error) &&
            world.State().enemyGroups[0].leaderId == "leader" &&
            !world.State().enemyGroups[0].hasLastKnownPosition,
            "Reload restores original leader with fresh memory");
        world.Initialize();
        manifest.enemyGroups.clear();
        check(Tank::App::SpawnManifestEnemies(world, manifest, catalog, error) && world.State().enemyGroups.empty(),
            "Legacy ungrouped maps still spawn normally");
        auto attacker = EnemyAttackType{};
        attacker.firingIntervalSeconds = 0.1f;
        attacker.maximumYawSpeedDegreesPerSecond = 360;
        attacker.projectileSpeedMetersPerSecond = 100;
        world.Initialize();
        advance(world, 180);
        check(world.ConfigureEnemyAttacks({attacker}) &&
            world.AddFixedTurret({0, 1.5f, 12}, {1, 1, 1}, 60, {0, 1, 0, 0}, "attacker") &&
            world.ConfigureEnemyGroups({{"combat", {{"attacker", 10}}}}), "Set up player loss scenario");
        advance(world, 30);
        check(world.State().playerCombat.phase == PlayerCombatPhase::Lost &&
            !world.State().enemyGroups[0].detected && world.State().enemyGroups[0].hasLastKnownPosition,
            "Player loss immediately clears current detection but keeps memory");
        const double beforeRespawn = world.State().enemyGroups[0].lastObservedSeconds;
        advance(world, 100);
        check(world.State().playerCombat.phase == PlayerCombatPhase::Alive &&
            world.State().enemyGroups[0].lastObservedSeconds > beforeRespawn,
            "Shared observation clock stays monotonic across player respawn");
        world.Initialize();
        manifest.enemies[1].unitType = "Fixed turret"; // Both observers have only a 1 m detection range.
        manifest.enemyGroups = {{"patrol", {{"leader", 10}, {"observer", 20}}}};
        manifest.enemyGroups[0].patrol = Tank::Map::EnemyPatrolSettings{
            {{{0, 1.5f, 20}, "ground"}}, 5, 7, 1, 3};
        check(Tank::App::SpawnManifestEnemies(world, manifest, catalog, error), "Spawn patrol navigation fixture");
        NavigationGrid grid;
        for (int z = 0; z < 3; ++z)
            grid.cells.push_back({0, z, "ground", {0, 1.5f, 12 + 4.0f * z}, NavigationTerrain::Road,
                10, 10, 0, 0, true});
        grid.links = {{0, 1, {10, 10, 0}}, {1, 2, {10, 10, 0}}};
        NavigationMobility mobility{2, 2, 2, 30, 1, 0.5f, {1, 1, 2, 4}};
        check(world.ConfigureGroupNavigation("patrol", grid, std::span(&mobility, 1), 0.5, "ground"),
            "Bind authored surfaces to runtime group");
        advance(world, 2);
        const auto& plan = world.State().enemyGroups[0].navigation;
        check(plan && !plan->stopped && plan->path && plan->path->cells.size() == 3 && plan->searches == 1,
            "Runtime patrol resolves goal and reuses shared path");
        check(!world.ConfigureGroupNavigation("patrol", grid, std::span(&mobility, 1), -1, "ground") &&
            world.State().enemyGroups[0].navigation->path.has_value(), "Invalid navigation replacement preserves active plan");
        world.Initialize();
        manifest.enemyGroups[0].patrol = Tank::Map::EnemyPatrolSettings{
            {{{0, 1.5f, 20}, "ground"}, {{0, 1.5f, 12}, "ground"}}, 5, 7, 0.1f, 3};
        check(Tank::App::SpawnManifestEnemies(world, manifest, catalog, error), "Spawn moving patrol");
        grid.cells[1].terrain = NavigationTerrain::Marsh;
        grid.links.push_back({2, 1, {10, 10, 0}});
        grid.links.push_back({1, 0, {10, 10, 0}});
        EnemyNavigationSetup setup{grid, {{"patrol", "leader", "ground", 0.1, mobility}}};
        check(Tank::App::ValidateEnemyNavigationSetup(setup, manifest, error), "Validate complete patrol cycle");
        auto oneWay = setup;
        oneWay.grid.links.resize(2);
        check(!Tank::App::ValidateEnemyNavigationSetup(oneWay, manifest, error), "Reject missing return route");
        auto wrongBinding = setup;
        wrongBinding.drivers[0].enemyId = "missing";
        check(!Tank::App::ValidateEnemyNavigationSetup(wrongBinding, manifest, error), "Reject invalid movement binding");
        auto retained = setup;
        check(!DeserializeEnemyNavigationSetup("{}", retained, error) && retained.drivers[0].enemyId == "leader",
            "Invalid navigation JSON preserves previous setup");
        check(world.ConfigureEnemyMovement(setup, error), "Configure actual movement");
        advance(world, 1);
        const auto start = world.State().fixedTurrets[0].position.z;
        advance(world, 300);
        check(std::abs(world.State().fixedTurrets[0].position.z - 16) < 0.1f && start == 12,
            "Terrain cost determines actual position after five seconds");
        check(world.State().fixedTurrets[1].position.x == 8 && world.State().fixedTurrets[1].position.z == 12,
            "Unconfigured fixed turret stays stationary");
        advance(world, 300);
        check(std::abs(world.State().fixedTurrets[0].position.z - 20) < 0.1f,
            "Moving enemy reaches authored patrol waypoint");
        const auto pivotPosition = world.State().fixedTurrets[0].position;
        advance(world, 60);
        const auto& turning = world.State().fixedTurrets[0];
        check(std::abs(turning.position.z-pivotPosition.z) < 0.001f &&
            std::abs(turning.rotation.y) > 0.4f && std::abs(turning.rotation.y) < 0.6f &&
            std::abs(turning.navigationVelocity.z) < 0.001f && turning.navigationYawRadiansPerSecond > 0,
            "Simple patrol pivots gradually in place instead of snapping 180 degrees");
        advance(world, 120);
        advance(world, 300);
        check(std::abs(world.State().fixedTurrets[0].position.z - 16) < 0.15f,
            "Patrol proceeds along return route");
        for (int shot = 0; shot < 3; ++shot)
        {
            check(world.FireAssault(), "Shoot moving enemy");
            advance(world, 20);
        }
        check(!world.State().fixedTurrets[0].target.active, "Moving enemy can be destroyed");
        const auto stoppedPosition = world.State().fixedTurrets[0].position.z;
        advance(world, 60);
        check(world.State().fixedTurrets[0].position.z == stoppedPosition &&
            world.State().enemyGroups[0].behavior->action == EnemyGroupAction::Hold,
            "Destroyed movement driver stops despite surviving group leader");
        world.Initialize();
        check(Tank::App::SpawnManifestEnemies(world, manifest, catalog, error) &&
            !world.State().fixedTurrets[0].simpleMoving && world.ConfigureEnemyMovement(setup, error),
            "Reset restores authored position and permits fresh movement setup");
        check(world.SetEnemySimulationSettings({30, 40}), "Apply live near-distance tuning");
        advance(world, 1);
        check(world.State().fixedTurrets[0].physicalMoving && world.State().fixedTurrets[0].runningGear &&
            world.State().fixedTurrets[0].runningGear->wheelCount > 0 && !world.State().fixedTurrets[1].runningGear &&
            !world.State().fixedTurrets[1].physicalMoving,
            "Nearby driver uses independent tracked controller; fixed turret remains static");
        check(!world.SetEnemySimulationSettings({40, 30}), "Reject inverted distance hysteresis");
        check(world.SetEnemySimulationSettings({1, 2}), "Change distances during play");
        advance(world, 1);
        check(world.State().fixedTurrets[0].physicalMoving, "Airborne enemy retains physics after live threshold change");
        check(world.SetEnemySimulationSettings({30, 40}), "Restore physical threshold");
        advance(world, 180);
        check(world.State().fixedTurrets[0].physicalMoving && world.State().fixedTurrets[0].position.y < 1.4f &&
            world.State().fixedTurrets[0].target.hitPoints == 60,
            "Tracked enemy responds to gravity and preserves combat HP");
        check(world.State().fixedTurrets[0].position.z > 12.1f, "Tracked driver advances along patrol route");
        bool reachedFarEnd = false, returned = false;
        float maximumZ = 0;
        for (int i = 0; i < 3600 && !returned; ++i)
        {
            advance(world, 1);
            const auto& moving = world.State().fixedTurrets[0];
            maximumZ = std::max(maximumZ, moving.position.z);
            if (moving.position.z > 19) reachedFarEnd = true;
            if (reachedFarEnd && moving.position.z < 15) returned = true;
        }
        check(reachedFarEnd && returned && maximumZ < 21.5f,
            "Physical patrol brakes, pivots, and returns without runaway overshoot");
        world.Initialize(); // Removes enemy body and its vehicle constraint safely.
        check(world.State().fixedTurrets.empty(), "Reset removes physical enemy controller");
        manifest.enemyGroups[0].patrol->points = {{{0, 1.5f, 12}, "ground"}};
        manifest.enemyGroups[0].patrol->arrivalMeters = 2;
        setup.drivers[0].snapDistanceMeters = 2;
        check(Tank::App::SpawnManifestEnemies(world, manifest, catalog, error) &&
            world.ConfigureEnemyMovement(setup, error) && world.SetEnemySimulationSettings({30, 40}),
            "Set up safely mapped return fixture");
        advance(world, 180);
        check(world.SetEnemySimulationSettings({1, 2}), "Adjust return threshold without reset");
        for (int i = 0; i < 180 && world.State().fixedTurrets[0].physicalMoving; ++i) advance(world, 1);
        check(!world.State().fixedTurrets[0].physicalMoving && !world.State().fixedTurrets[0].runningGear, "Stable mapped enemy returns to simple movement after live tuning");
        check(world.SetEnemySimulationSettings({30, 40}), "Re-enter physical range");
        advance(world, 1);
        auto lowMuzzle = world.ProjectileSettings();
        lowMuzzle.muzzleLocalPosition.y = 0; // Aim through the physical chassis, below the visual turret.
        world.SetAssaultProjectileSettings(lowMuzzle);
        for (int shot = 0; shot < 3; ++shot)
        {
            check(world.FireAssault(), "Shoot physical enemy body");
            advance(world, 20);
        }
        check(!world.State().fixedTurrets[0].target.active, "Physical enemy body receives combat damage");
        advance(world, 2);
        check(!world.State().fixedTurrets[0].physicalMoving, "Destroyed physical enemy releases its controller");
        world.Initialize();
        advance(world, 180);
        catalog.attackTypes[0].detectionRangeMeters = 50;
        check(Tank::App::SpawnManifestEnemies(world, manifest, catalog, error), "Spawn pursuit fixture");
        NavigationGrid chaseGrid;
        for (int z = 0; z < 6; ++z)
        {
            chaseGrid.cells.push_back({0, z, "ground", {0, 1.5f, 4.0f*z}, NavigationTerrain::Road,
                10, 10, 0, 0, true, 0.0f});
            if (z > 0) { chaseGrid.links.push_back({size_t(z-1), size_t(z), {10, 10, 0}});
                chaseGrid.links.push_back({size_t(z), size_t(z-1), {10, 10, 0}}); }
        }
        setup.grid = chaseGrid;
        setup.drivers[0].snapDistanceMeters = 0.1;
        check(world.ConfigureEnemyMovement(setup, error) && world.SetEnemySimulationSettings({1, 2}),
            "Configure simple pursuit on authored support heights");
        advance(world, 1);
        check(world.State().enemyGroups[0].detected && world.State().enemyGroups[0].lastNavigationObservation &&
            world.State().enemyGroups[0].behavior->targetSurfaceId == "ground" &&
            !world.State().enemyGroups[0].navigation->stopped, "Observed physical support produces pursuit route");
        advance(world, 420);
        check(world.State().fixedTurrets[0].position.z < 6 && world.State().fixedTurrets[0].position.z >= 4.9f &&
            world.State().enemyGroups[0].behavior->action == EnemyGroupAction::AttackStop,
            "Pursuit moves enemy and stops at authored attack distance");
        check(world.AddDestructibleBox({0, 1.5f, 2}, {30, 5, 1}), "Occlude pursued player");
        advance(world, 1);
        check(!world.State().enemyGroups[0].detected &&
            world.State().enemyGroups[0].behavior->action == EnemyGroupAction::Search &&
            world.State().enemyGroups[0].behavior->targetSurfaceId == "ground" &&
            world.State().enemyGroups[0].lastNavigationObservation->surfaceId == "ground" &&
            !world.State().enemyGroups[0].navigation->stopped,
            "Search retains last observed support surface after occlusion");
        world.Initialize();
        catalog.attackTypes[0].detectionRangeMeters = 1;
        manifest.enemyGroups[0].patrol->points = {{{0, 1.5f, 20}, "ground"}, {{0, 1.5f, 12}, "ground"}};
        manifest.enemyGroups[0].patrol->arrivalMeters = 0.1f;
        check(Tank::App::SpawnManifestEnemies(world, manifest, catalog, error) &&
            world.ConfigureEnemyMovement(setup, error), "Configure movement toggle fixture");
        EnemySimulationSettings movementToggle{1, 2};
        movementToggle.movementEnabled = false;
        check(world.SetEnemySimulationSettings(movementToggle), "Start with enemy movement disabled");
        const auto disabledStart = world.State().fixedTurrets[0].position;
        advance(world, 180);
        check(NavigationDistance(world.State().fixedTurrets[0].position, disabledStart) < 0.00001f &&
            !world.State().fixedTurrets[0].simpleMoving && !world.State().fixedTurrets[0].physicalMoving,
            "Disabled authored patrol stays fixed and never enters tracked physics");
        movementToggle.movementEnabled = true;
        check(world.SetEnemySimulationSettings(movementToggle), "Enable movement live");
        advance(world, 120);
        check(world.State().fixedTurrets[0].position.z > disabledStart.z + 0.5f,
            "Enabling resumes simple patrol");
        const auto simpleStop = world.State().fixedTurrets[0].position;
        movementToggle.movementEnabled = false;
        check(world.SetEnemySimulationSettings(movementToggle), "Disable simple patrol live");
        advance(world, 180);
        check(NavigationDistance(world.State().fixedTurrets[0].position, simpleStop) < 0.00001f &&
            NavigationDistance(world.State().fixedTurrets[0].navigationVelocity, {}) < 0.00001f,
            "Disabling simple movement removes residual velocity");
        movementToggle.movementEnabled = true;
        check(world.SetEnemySimulationSettings(movementToggle), "Reenable simple patrol");
        advance(world, 60);
        check(NavigationDistance(world.State().fixedTurrets[0].position, simpleStop) > 0.1f,
            "Reenabled patrol continues from stopped pose");
        movementToggle.physicalEnterMeters = 30;
        movementToggle.simpleReturnMeters = 40;
        check(world.SetEnemySimulationSettings(movementToggle), "Enable nearby physical enemy");
        advance(world, 1);
        check(world.State().fixedTurrets[0].physicalMoving, "Toggle fixture enters tracked physics");
        const auto physicalStop = world.State().fixedTurrets[0].position;
        movementToggle.movementEnabled = false;
        check(world.SetEnemySimulationSettings(movementToggle), "Disable tracked physics movement live");
        advance(world, 120);
        check(NavigationDistance(world.State().fixedTurrets[0].position, physicalStop) < 0.00001f &&
            !world.State().fixedTurrets[0].physicalMoving && !world.State().fixedTurrets[0].runningGear &&
            world.State().fixedTurrets[0].target.hitPoints == 60,
            "Disabling physical movement fixes current pose and preserves combat HP");
        movementToggle.movementEnabled = true;
        check(world.SetEnemySimulationSettings(movementToggle), "Reenable physical movement without reset");
        advance(world, 1);
        check(world.State().fixedTurrets[0].physicalMoving && world.State().fixedTurrets[0].runningGear,
            "Physical controller can be recreated after OFF/ON");
        world.Initialize();
        advance(world, 180);
        catalog.attackTypes[0] = {};
        catalog.attackTypes[0].detectionMode = EnemyDetectionMode::RangeOnly;
        catalog.attackTypes[0].firingIntervalSeconds = 0.1f;
        catalog.attackTypes[0].maximumYawSpeedDegreesPerSecond = 360;
        movementToggle.movementEnabled = false;
        check(Tank::App::SpawnManifestEnemies(world, manifest, catalog, error) &&
            world.ConfigureEnemyMovement(setup, error) && world.SetEnemySimulationSettings(movementToggle),
            "Configure armed enemy with movement OFF");
        advance(world, 120);
        check(world.State().fixedTurrets[0].mounts[0].aim.detected &&
            world.State().fixedTurrets[0].mounts[0].aim.shotsFired > 0 &&
            std::abs(world.State().fixedTurrets[0].position.z - 12) < 0.00001f,
            "Movement OFF preserves detection, aiming and firing");
        world.Initialize();
        check(world.State().fixedTurrets.empty(), "Reset cleans up controllers after repeated movement toggles");
        std::cout << "PASS EnemyGroupIntegration\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAIL EnemyGroupIntegration: " << error.what() << '\n';
        return 1;
    }
}
