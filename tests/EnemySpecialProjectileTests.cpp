#include "Physics/TrackedVehicleTest.h"
#include <algorithm>
#include <iostream>
#include <cmath>
#include <vector>

namespace
{
    constexpr float dt=1.0f/60;
    void Advance(Tank::Physics::TrackedVehicleTest& world,int steps)
    {for(int i=0;i<steps;++i)world.Step(dt);}
}
int main()
{
    using namespace Tank::Physics;
    int failed=0;
    auto check=[&](bool ok,const char* name){if(!ok){std::cerr<<"FAIL "<<name<<'\n';++failed;}};
    TrackedVehicleTest world;
    EnemyAttackType special;
    special.projectileKind=EnemyProjectileKind::Special;
    special.projectileRadiusMeters=0.5f;
    special.maximumYawSpeedDegreesPerSecond=360;
    for (auto shape : {EnemyProjectileShape::Sphere, EnemyProjectileShape::Box})
    {
    special.projectileShape = shape;
    special.projectileBoxSizeMeters = {1, 0.5f, 2};
    special.firingIntervalSeconds = 3;
    special.projectileSpeedMetersPerSecond = 1;
    world.Initialize();Advance(world,180);world.ConfigureEnemyAttacks({special});
    world.AddFixedTurret({0,1.5f,12},{1,1,1},60,{0,1,0,0});
    Advance(world,240);
    check(!world.State().enemyProjectiles.empty(),"special round in flight");
    if(!world.State().enemyProjectiles.empty())
    {
        const auto id=world.State().enemyProjectiles.front().target.id;
        check(world.State().enemyProjectiles.front().target.kind==CombatTargetKind::EnemySpecialProjectile &&
            world.State().enemyProjectiles.front().radius==0.5f && world.State().enemyProjectiles.front().shape==shape &&
            world.State().enemyProjectiles.front().boxSizeMeters.z==2,"snapshot kind, shape and dimensions");
        auto projectile=world.ProjectileSettings();
        projectile.muzzleLocalPosition.y=1.5f-world.State().bodyPosition.y;
        world.SetAssaultProjectileSettings(projectile);world.FireAssault();Advance(world,12);
        check(std::any_of(world.State().enemyProjectiles.begin(),world.State().enemyProjectiles.end(),
            [id](const auto& bullet){return bullet.target.id==id && bullet.target.active;}),"normal fire cannot intercept special");
        check(world.State().fixedTurrets[0].target.hitPoints==40,"normal fire passes special to hit turret");
    }

    // A real mortar blast hits a turret with two different attack templates.
    TankSettings settings;
    settings.mortarMinimumFireAngleDegrees=0;
    settings.mortarMinimumRangeMeters=settings.mortarMaximumRangeMeters=15;
    settings.mortarMinimumAttackRadiusMeters=settings.mortarMaximumAttackRadiusMeters=4;
    world.Initialize(settings);Advance(world,60);
    special.firingIntervalSeconds=0.1f;special.projectileSpeedMetersPerSecond=0.1f;
    auto ordinary=special;ordinary.name="ordinary";ordinary.projectileKind=EnemyProjectileKind::Ordinary;
    ordinary.projectileRadiusMeters=0.25f;ordinary.projectileShape=EnemyProjectileShape::Sphere;
    world.ConfigureEnemyAttacks({ordinary,special});
    EnemyUnitType unit;unit.attackMounts={{{2,1,0},0},{{-2,1,0},1}};
    world.AddFixedTurret({0,0.5f,15},{1,1,1},60,{0,1,0,0},"mixed",unit);Advance(world,30);
    std::vector<std::uint64_t> ordinaryIds,specialIds;
    for(const auto& bullet:world.State().enemyProjectiles)
        (bullet.target.kind==CombatTargetKind::EnemySpecialProjectile ? specialIds : ordinaryIds).push_back(bullet.target.id);
    check(!ordinaryIds.empty() && !specialIds.empty(),"mixed ordinary and special rounds");
    TankInput input;input.leftLeverX=-1;input.rightLeverX=1;world.SetInput(input);Advance(world,10);
    input.fireAssault=true;world.SetInput(input);world.Step(dt);world.SetInput({});
    for(int i=0;i<600 && world.State().mortarBlasts.empty();++i)world.Step(dt);
    check(!world.State().mortarBlasts.empty(),"mortar actually landed");
    auto exists=[&](std::uint64_t id){return std::any_of(world.State().enemyProjectiles.begin(),world.State().enemyProjectiles.end(),
        [id](const auto& bullet){return bullet.target.id==id;});};
    check(std::none_of(ordinaryIds.begin(),ordinaryIds.end(),exists),"mortar destroys ordinary rounds");
    check(std::all_of(specialIds.begin(),specialIds.end(),exists),"mortar preserves special rounds");
    check(!world.State().fixedTurrets[0].target.active,"mortar still destroys firing turret");

    }
    special.projectileShape=EnemyProjectileShape::Sphere;
    // Sphere size must affect swept terrain contact, including an off-axis thin blocker.
    special.projectileSpeedMetersPerSecond=100;
    for(float radius:{0.25f,1.0f})
    {
        special.projectileRadiusMeters=radius;
        world.Initialize();Advance(world,180);world.ConfigureEnemyAttacks({special});
        world.AddFixedTurret({0,1.5f,12},{1,1,1},60,{0,1,0,0});Advance(world,6);
        check(!world.State().enemyProjectiles.empty(),"sphere spawned before blocker");
        world.AddDestructibleBox({0.85f,1.5f,6},{0.1f,3,0.1f},60);Advance(world,12);
        if(radius==1)
            check(world.State().playerCombat.hitPoints==100,"large swept sphere contacts off-axis blocker");
        else check(world.State().playerCombat.phase==PlayerCombatPhase::Lost && world.State().playerCombat.lives==2,
            "small special sphere passes blocker and deals 100 damage");
    }
    // A diagonal long box must use its oriented width, not a sphere or an axis-aligned proxy.
    special.projectileShape=EnemyProjectileShape::Box;
    for(float width:{0.2f,2.0f})
    {
        special.projectileBoxSizeMeters={width,0.5f,2};
        world.Initialize();Advance(world,180);world.ConfigureEnemyAttacks({special});
        const float yaw=3.14159265f*0.75f;
        world.AddFixedTurret({-12,1.5f,12},{1,1,1},60,{0,std::sin(yaw*0.5f),0,std::cos(yaw*0.5f)});
        Advance(world,6);
        check(!world.State().enemyProjectiles.empty(),"diagonal box spawned");
        world.AddDestructibleBox({-6.424264f,1.5f,5.575736f},{0.1f,3,0.1f},60);Advance(world,16);
        if(width==2)check(world.State().playerCombat.hitPoints==100,"wide oriented box touches diagonal off-axis blocker");
        else check(world.State().playerCombat.phase==PlayerCombatPhase::Lost,"narrow oriented box misses blocker and hits tank");
    }
    // Direction alignment must be normalized even for vertical and backward shots.
    for(const Vec3 direction : {Vec3{1,2,-3},Vec3{0,1,0},Vec3{0,-1,0},Vec3{0,0,-1}})
    {
        const auto q=EnemyProjectileRotation(direction);
        const Vec3 forward={2*(q.x*q.z+q.y*q.w),2*(q.y*q.z-q.x*q.w),1-2*(q.x*q.x+q.y*q.y)};
        const float length=std::hypot(direction.x,direction.y,direction.z);
        check(std::abs(forward.x-direction.x/length)<0.0001f && std::abs(forward.y-direction.y/length)<0.0001f &&
            std::abs(forward.z-direction.z/length)<0.0001f,"box flight orientation");
    }
    if(!failed)std::cout<<"PASS EnemySpecialProjectile\n";
    return failed ? 1 : 0;
}
