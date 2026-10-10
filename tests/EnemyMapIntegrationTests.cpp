#include "App/ManifestEnemySpawner.h"
#include "Map/GltfHitMesh.h"
#include "Physics/EnemyEditorJson.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>

int main()
{
    try
    {
        auto check = [](bool ok, const char* message) { if (!ok) throw std::runtime_error(message); };
        const auto folder = std::filesystem::path(__FILE__).parent_path().parent_path() / "Assets/Map/WD2EnemyRange";
        auto read = [](const std::filesystem::path& path) {
            std::ifstream input(path, std::ios::binary);
            if (!input) throw std::runtime_error("Cannot open sample asset");
            return std::string(std::istreambuf_iterator<char>(input), {});
        };
        std::string error;
        Tank::Map::Manifest manifest;
        Tank::Physics::EnemyEditorSettings catalog;
        check(Tank::Map::DeserializeManifest(read(folder / "Manifest.json"), manifest, error), "Read sample map");
        check(Tank::Physics::DeserializeEnemyEditor(read(folder / "enemy_types.json"), catalog, error), "Read local templates");
        check(manifest.enemies.size() == 4 && catalog.attackTypes.size() == 3, "All sample stations exist");
        std::string saved;
        check(Tank::Map::SerializeManifest(manifest, saved, error), "Serialize placements");
        Tank::Map::Manifest reopened;
        check(Tank::Map::DeserializeManifest(saved, reopened, error) && reopened.enemies.size() == 4 &&
            reopened.enemies[3].unitType == "TurretMixed", "Placement references survive save/load");
        std::vector<Tank::Map::HitTriangleMesh> meshes;
        for (const auto& instance : reopened.instances)
        {
            Tank::Map::HitTriangleMesh local, world;
            check(Tank::Map::LoadGltfHitMesh(folder / instance.asset, local, error), "Read sample ground");
            check(Tank::Map::TransformHitMesh(local, instance.transform, world, error), "Scale sample ground");
            meshes.push_back(std::move(world));
        }
        Tank::Physics::TrackedVehicleTest vehicle;
        Tank::Physics::MapSpawn spawn;
        spawn.position = {0, 2, -20};
        auto reset = [&] {
            check(vehicle.InitializeWithStaticMeshes({}, {}, meshes, spawn, error), "Initialize authored floor");
            check(Tank::App::SpawnManifestEnemies(vehicle, reopened, catalog, error), "Place named enemy templates");
        };
        reset();
        for (int i = 0; i < 360; ++i) vehicle.Step(1.0f / 60);
        bool ordinary = false, sphere = false, box = false;
        for (const auto& bullet : vehicle.State().enemyProjectiles)
        {
            ordinary |= bullet.target.kind == Tank::Physics::CombatTargetKind::EnemyProjectile;
            sphere |= bullet.target.kind == Tank::Physics::CombatTargetKind::EnemySpecialProjectile && bullet.shape == Tank::Physics::EnemyProjectileShape::Sphere;
            box |= bullet.target.kind == Tank::Physics::CombatTargetKind::EnemySpecialProjectile && bullet.shape == Tank::Physics::EnemyProjectileShape::Box;
        }
        check(ordinary && sphere && box, "All three sample attacks fire with default parameters");
        check(vehicle.State().fixedTurrets[3].mounts.size() == 3, "Mixed turret has three independent mounts");
        for (int i = 0; i < 18000 && vehicle.State().playerCombat.phase != Tank::Physics::PlayerCombatPhase::GameOver; ++i)
            vehicle.Step(1.0f / 60);
        check(vehicle.State().playerCombat.phase == Tank::Physics::PlayerCombatPhase::GameOver &&
            vehicle.State().playerCombat.lives == 0, "Three losses reach GameOver on sample map");
        reset();
        check(vehicle.State().playerCombat.lives == 3 && vehicle.State().playerCombat.hitPoints == 100 &&
            vehicle.State().bodyPosition.z == -20 && vehicle.State().fixedTurrets.size() == 4,
            "Continue reset restores map start and enemies");
        return 0;
    }
    catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
