#include "Physics/AssaultWeapon.h"

#include <cmath>
#include <iostream>

namespace
{
    bool Check(bool condition, const char* message)
    {
        if (!condition) std::cerr << "FAIL Assault weapon: " << message << "\n";
        return condition;
    }
}

int main()
{
    Tank::Physics::AssaultWeapon weapon({20.0f, 8.0f});
    bool passed = true;
    passed &= Check(weapon.TryFire(), "first round must fire immediately");
    passed &= Check(!weapon.TryFire(), "weapon must enforce its fire interval");
    weapon.Update(0.124f);
    passed &= Check(!weapon.TryFire(), "weapon must not fire early");
    weapon.Update(0.001f);
    passed &= Check(weapon.TryFire(), "weapon must sustain automatic fire");

    Tank::Physics::CombatTarget enemy {1, Tank::Physics::CombatTargetKind::Enemy, 35.0f, true};
    const auto firstEnemyHit = weapon.ApplyHit(enemy);
    passed &= Check(firstEnemyHit.hit && !firstEnemyHit.destroyed && enemy.hitPoints == 15.0f,
        "ordinary enemies must take medium damage");
    const auto finalEnemyHit = weapon.ApplyHit(enemy);
    passed &= Check(finalEnemyHit.destroyed && !enemy.active,
        "enemies must be destroyed at zero health");

    Tank::Physics::CombatTarget crate {2, Tank::Physics::CombatTargetKind::Destructible, 20.0f, true};
    passed &= Check(weapon.ApplyHit(crate).destroyed && !crate.active,
        "destructible objects must use the same damage contract");

    Tank::Physics::CombatTarget projectile {3, Tank::Physics::CombatTargetKind::EnemyProjectile, 1.0f, true};
    const auto interception = weapon.ApplyHit(projectile);
    passed &= Check(interception.interceptedProjectile && interception.destroyed && !projectile.active,
        "enemy projectiles must be interceptable");

    if (!passed) return 1;
    std::cout << "PASS AssaultWeapon rounds=" << weapon.Snapshot().roundsFired << "\n";
    return 0;
}
