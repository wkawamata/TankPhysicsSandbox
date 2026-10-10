#pragma once

#include "Map/MapManifest.h"
#include "Physics/EnemyAttackType.h"
#include "Physics/TrackedVehicleTest.h"
#include <algorithm>
#include <cmath>

namespace Tank::App
{
    // Validate all references before adding any bodies. No rendering dependency.
    inline bool SpawnManifestEnemies(Physics::TrackedVehicleTest& world,
        const Map::Manifest& manifest, const Physics::EnemyEditorSettings& catalog, std::string& error)
    {
        error.clear();
        if (!world.State().fixedTurrets.empty())
        { error = "Enemy world must be empty before placement"; return false; }
        std::vector<const Physics::EnemyUnitType*> types;
        for (const auto& enemy : manifest.enemies)
        {
            const auto type = std::find_if(catalog.unitTypes.begin(), catalog.unitTypes.end(),
                [&](const auto& value) { return value.name == enemy.unitType; });
            if (type == catalog.unitTypes.end())
            { error = "Unknown enemy type '" + enemy.unitType + "' for " + enemy.id; return false; }
            types.push_back(&*type);
        }
        if (!world.ConfigureEnemyAttacks(catalog.attackTypes))
        { error = "Invalid enemy attack catalog"; return false; }
        for (size_t i = 0; i < manifest.enemies.size(); ++i)
        {
            const auto& enemy = manifest.enemies[i];
            const auto& p = enemy.position;
            const auto& a = enemy.rotationDegrees;
            constexpr float halfRadians = 3.14159265358979323846f / 360.0f;
            const float sp = std::sin(a[0] * halfRadians), cp = std::cos(a[0] * halfRadians);
            const float sy = std::sin(a[1] * halfRadians), cy = std::cos(a[1] * halfRadians);
            const float sr = std::sin(a[2] * halfRadians), cr = std::cos(a[2] * halfRadians);
            // Yaw * pitch * roll, matching the map's DirectX placement convention.
            const Physics::Quat q = {cr * sp * cy + sr * cp * sy, cr * cp * sy - sr * sp * cy,
                sr * cp * cy - cr * sp * sy, cr * cp * cy + sr * sp * sy};
            // Same 1 m proxy as editor preview; existing destructible default HP.
            if (!world.AddFixedTurret({p[0], p[1], p[2]}, {1, 1, 1}, 60, q, enemy.id, *types[i]))
            { error = "Cannot spawn enemy " + enemy.id; return false; }
        }
        return true;
    }
}
