#include "Physics/TrackedVehicleTest.h"
#include <cmath>
#include <iostream>
#include <limits>

using namespace Tank::Physics;
namespace { void Advance(TrackedVehicleTest& world, int steps) { for (int i=0;i<steps;++i) world.Step(1.0f/60); } }
int main()
{
    bool passed = true;
    auto check = [&](bool ok, const char* message) { if (!ok) { passed=false; std::cerr << "FAIL EnemyCombat: " << message << '\n'; } };
    EnemyAttackType type;
    EnemyAimState aim;
    aim.secondsUntilNextShot=3;
    check(!UpdateEnemyAim(aim,type,{}, {0,0,51},1,true,true) && !aim.detected, "outside detection");
    check(!UpdateEnemyAim(aim,type,{}, {0,0,50},1,true,true) && aim.detected && !aim.withinReach, "detection boundary");
    check(!UpdateEnemyAim(aim,type,{}, {0,0,30},1,true,true) && aim.withinReach && !aim.canFire, "reach separate from firing");
    check(UpdateEnemyAim(aim,type,{}, {0,0,25},1,true,true) && aim.canFire, "firing boundary and first interval");
    check(!UpdateEnemyAim(aim,type,{}, {0,0,25},1,true,true), "cooldown");
    check(!UpdateEnemyAim(aim,type,{}, {0,0,25},1,false,true) && !aim.detected, "occlusion clears recognition");
    aim.yawRadians=0; aim.secondsUntilNextShot=0;
    check(!UpdateEnemyAim(aim,type,{}, {10,0,0},1,true,true) && std::abs(aim.yawRadians-3.14159265f/36)<1e-5f, "maximum yaw speed");
    aim.yawRadians=179*3.14159265f/180;
    UpdateEnemyAim(aim,type,{}, {-0.174524f,0,-9.998477f},1,true,true);
    check(std::abs(WrapEnemyAngle(aim.yawRadians+179*3.14159265f/180))<1e-4f, "shortest turn across 180");
    check(!UpdateEnemyAim(aim,type,{}, {},1,true,false) && !aim.canFire, "destroyed unit cannot fire");
    aim.yawRadians=0;aim.secondsUntilNextShot=0;
    check(UpdateEnemyAim(aim,type,{}, {std::sin(3*3.14159265f/180)*10,0,std::cos(3*3.14159265f/180)*10},0.000001f,true,true),
        "positive tolerance boundary fires");
    aim.yawRadians=0;aim.secondsUntilNextShot=0;
    check(UpdateEnemyAim(aim,type,{}, {-std::sin(3*3.14159265f/180)*10,0,std::cos(3*3.14159265f/180)*10},0.000001f,true,true),
        "negative tolerance boundary fires");
    aim.yawRadians=0;aim.secondsUntilNextShot=0;
    check(!UpdateEnemyAim(aim,type,{}, {std::sin(3.01f*3.14159265f/180)*10,0,std::cos(3.01f*3.14159265f/180)*10},0.000001f,true,true),
        "outside tolerance waits");
    EnemyProjectileState bullet;
    bullet.position={0,0,0}; bullet.velocity={0,0,100};
    check(EnemyInterceptionFraction({-10,0,5},{10,0,5},bullet,0.1f)<1, "crossing fast projectiles swept interception");
    check(EnemyInterceptionFraction({-10,2,5},{10,2,5},bullet,0.1f)>1, "off-axis miss");

    TrackedVehicleTest world;
    world.Initialize(); Advance(world,180);
    type.firingIntervalSeconds=0.1f; type.maximumYawSpeedDegreesPerSecond=360;
    type.projectileSpeedMetersPerSecond=100;
    check(world.ConfigureEnemyAttacks({type}), "configure catalog");
    EnemyUnitType unit;
    unit.attackMounts.push_back({{0.2f,0,0},0});
    check(world.AddFixedTurret({0,1.5f,12},{1,1,1},60,{0,1,0,0},"enemy-1",unit), "multiple mounts");
    Advance(world,30);
    check(world.State().fixedTurrets[0].mounts.size()==2 && world.State().fixedTurrets[0].mounts[0].aim.shotsFired>0,
        "independent mount fire state");
    check(world.State().playerCombat.phase==PlayerCombatPhase::Lost && world.State().playerCombat.hitPoints==0 &&
        world.State().playerCombat.lives==2, "100 damage loses exactly one life despite simultaneous hits");
    check(world.State().enemyProjectiles.empty(), "hit bullets removed");

    const auto lost = world.State().playerCombat.lostPosition;
    const auto turretId = world.State().fixedTurrets[0].target.id;
    check(!world.FireAssault(), "Lost rejects shooting");
    const int waitingSteps = std::max(0,static_cast<int>(std::floor(world.State().respawnSecondsRemaining*60))-1);
    Advance(world,waitingSteps);
    check(world.State().playerCombat.phase==PlayerCombatPhase::Lost, "waits one second before respawn");
    for(int i=0;i<3 && world.State().playerCombat.phase==PlayerCombatPhase::Lost;++i)Advance(world,1);
    check(world.State().playerCombat.phase==PlayerCombatPhase::Alive && world.State().playerCombat.hitPoints==100 &&
        world.State().playerCombat.lives==2 && world.State().invulnerabilitySecondsRemaining==2,
        "respawn restores HP with two second immunity");
    check(world.State().bodyPosition.x==lost.x && world.State().bodyPosition.y==lost.y && world.State().bodyPosition.z==lost.z &&
        world.State().fixedTurrets[0].target.id==turretId, "respawn uses lost position and preserves enemies");
    TankInput held;held.fireAssault=true;world.SetInput(held);
    check(!world.FireAssault(), "held trigger must be released after respawn");world.SetInput({});
    Advance(world,119);
    check(world.State().playerCombat.phase==PlayerCombatPhase::Alive && world.State().playerCombat.lives==2,
        "incoming fire cannot damage during immunity");
    for(int i=0;i<2000 && world.State().playerCombat.phase!=PlayerCombatPhase::GameOver;++i)Advance(world,1);
    check(world.State().playerCombat.phase==PlayerCombatPhase::GameOver && world.State().playerCombat.lives==0 &&
        world.State().respawnCount==2, "third loss reaches GameOver without respawn");
    const auto frozenStep=world.State().stepIndex;Advance(world,120);
    check(world.State().stepIndex==frozenStep && !world.FireAssault(), "GameOver stops world and shooting");
    world.Initialize({}, {}, {}, {{3,2,4},0});
    check(world.State().bodyPosition.x==3 && world.State().bodyPosition.y==2 && world.State().bodyPosition.z==4,
        "restart snapshot shows spawn before first physics step");
    Advance(world,1);
    check(world.State().playerCombat.hitPoints==100 && world.State().playerCombat.lives==3 &&
        world.State().fixedTurrets.empty() && world.State().bodyPosition.x==3 && world.State().bodyPosition.z==4,
        "full restart restores map start and lives");

    MapPrimitive wall; wall.position={0,1.5f,6};wall.size={4,3,0.2f};
    world.Initialize({}, {}, {wall}); Advance(world,180); world.ConfigureEnemyAttacks({type});
    world.AddFixedTurret({0,1.5f,12},{1,1,1},60,{0,1,0,0}); Advance(world,120);
    check(!world.State().fixedTurrets[0].mounts[0].aim.detected && world.State().enemyProjectiles.empty(), "terrain blocks detection and firing");

    world.Initialize(); Advance(world,180);world.ConfigureEnemyAttacks({type});
    world.AddFixedTurret({0,1.5f,12},{1,1,1},60,{0,1,0,0});Advance(world,6);
    check(!world.State().enemyProjectiles.empty(), "round in flight before blocker");
    world.AddDestructibleBox({0,1.5f,6},{4,3,0.2f},60);Advance(world,20);
    check(world.State().enemyProjectiles.empty() && world.State().playerCombat.hitPoints==100,
        "swept sphere hits thin blocker before player");

    world.Initialize(); Advance(world,180); type.firingIntervalSeconds=3;type.projectileSpeedMetersPerSecond=1;
    world.ConfigureEnemyAttacks({type});world.AddFixedTurret({0,1.5f,12},{1,1,1},60,{0,1,0,0});
    Advance(world,180);
    check(!world.State().enemyProjectiles.empty(), "slow enemy round spawns");
    Advance(world,60); // The round must leave its owner collider before interception.
    if (!world.State().enemyProjectiles.empty())
    {
        const auto id=world.State().enemyProjectiles[0].target.id;
        auto settings=world.ProjectileSettings(); settings.muzzleLocalPosition.y=1.5f-world.State().bodyPosition.y;
        world.SetAssaultProjectileSettings(settings);world.FireAssault();Advance(world,12);
        bool removed=true; for (const auto& p:world.State().enemyProjectiles) if(p.target.id==id)removed=false;
        check(removed && world.State().fixedTurrets[0].target.hitPoints==60, "normal shot intercepts enemy round before turret");
    }
    world.Initialize(); Advance(world,180); type.firingIntervalSeconds=10;
    world.ConfigureEnemyAttacks({type});world.AddFixedTurret({0,1.5f,12},{1,1,1},60,{0,1,0,0});
    for(int i=0;i<3;++i){world.FireAssault();Advance(world,20);} Advance(world,700);
    check(!world.State().fixedTurrets[0].target.active && world.State().enemyProjectiles.empty(), "destroyed turret stops firing");
    world.Initialize(); Advance(world,180); type.firingIntervalSeconds=0.001f; type.projectileSpeedMetersPerSecond=0.001f;
    world.ConfigureEnemyAttacks({type});world.AddFixedTurret({0,1.5f,12},{1,1,1},60,{0,1,0,0});Advance(world,400);
    check(world.State().enemyProjectiles.size()==kMaximumEnemyProjectiles, "bounded enemy projectile count");
    world.Initialize(); Advance(world,180); type.firingIntervalSeconds=1; type.reachMeters=5;type.firingRangeMeters=5;
    type.projectileSpeedMetersPerSecond=10;
    world.ConfigureEnemyAttacks({type});world.AddFixedTurret({0,20,0},{1,1,1},60);Advance(world,60);
    const auto firstId=world.State().enemyProjectiles.empty()?0:world.State().enemyProjectiles[0].target.id;
    Advance(world,31);
    check(firstId!=0 && world.State().enemyProjectiles.empty() && world.State().playerCombat.hitPoints==100,
        "reach expires airborne projectile before distant vertical target");
    if(!passed)return 1;
    std::cout << "PASS EnemyCombat\n";
}
