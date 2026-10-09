#include "Physics/EnemyAttackType.h"
#include <iostream>
#include <limits>

int main()
{
    using namespace Tank::Physics;
    EnemyEditorSettings editor;
    const auto& type = editor.attackTypes[0];
    bool ok = type.detectionRangeMeters == 50 && type.reachMeters == 30 && type.firingRangeMeters == 25 &&
        type.projectileSpeedMetersPerSecond == 1 && type.firingIntervalSeconds == 3 &&
        type.maximumYawSpeedDegreesPerSecond == 5 && type.firingToleranceDegrees == 3 && IsValidEnemyAttackType(type);
    auto invalid = type;
    invalid.reachMeters = 51;
    ok &= !IsValidEnemyAttackType(invalid);
    invalid = type;
    invalid.firingRangeMeters = 31;
    ok &= !IsValidEnemyAttackType(invalid);
    invalid = type;
    invalid.firingIntervalSeconds = 0;
    ok &= !IsValidEnemyAttackType(invalid);
    invalid = type;
    invalid.projectileSpeedMetersPerSecond = std::numeric_limits<float>::infinity();
    ok &= !IsValidEnemyAttackType(invalid);
    invalid = type;
    invalid.projectileRadiusMeters = -1;
    ok &= !IsValidEnemyAttackType(invalid);
    invalid.projectileRadiusMeters = std::numeric_limits<float>::quiet_NaN();
    ok &= !IsValidEnemyAttackType(invalid);
    invalid = type;
    invalid.projectileKind = static_cast<EnemyProjectileKind>(99);
    ok &= !IsValidEnemyAttackType(invalid);
    invalid = type;
    invalid.projectileShape = EnemyProjectileShape::Box;
    ok &= !IsValidEnemyAttackType(invalid); // Ordinary bullets remain spherical.
    invalid.projectileKind = EnemyProjectileKind::Special;
    ok &= IsValidEnemyAttackType(invalid);
    invalid.projectileBoxSizeMeters.x = 0;
    ok &= !IsValidEnemyAttackType(invalid);
    invalid.projectileBoxSizeMeters = {1, 1, std::numeric_limits<float>::infinity()};
    ok &= !IsValidEnemyAttackType(invalid);
    invalid = type;
    invalid.projectileShape = static_cast<EnemyProjectileShape>(99);
    ok &= !IsValidEnemyAttackType(invalid);
    editor.attackTypes.push_back(type);
    editor.attackTypes[1].projectileSpeedMetersPerSecond = 2;
    editor.unitTypes[0].attackMounts.push_back({{1, 0, 0}, 1});
    ok &= editor.unitTypes[0].attackMounts[0].attackTypeIndex == 0 &&
        editor.unitTypes[0].attackMounts[1].attackTypeIndex == 1 && editor.attackTypes[0].projectileSpeedMetersPerSecond == 1;
    if (!ok) { std::cerr << "FAIL EnemyAttackType\n"; return 1; }
    std::cout << "PASS EnemyAttackType\n";
}
