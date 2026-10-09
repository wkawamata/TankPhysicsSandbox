#include "Physics/TrackedVehicleTest.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <vector>

namespace
{
    constexpr float dt=1.0f/60;
    void Advance(Tank::Physics::TrackedVehicleTest& world,int steps)
    { for(int i=0;i<steps;++i)world.Step(dt); }
    bool Launch(Tank::Physics::TrackedVehicleTest& world)
    {
        Tank::Physics::TankInput input;
        input.leftLeverX=-1;input.rightLeverX=1;
        world.SetInput(input);Advance(world,10);
        input.fireAssault=true;world.SetInput(input);world.Step(dt);
        world.SetInput({});
        return world.State().mortarProjectiles.size()==1;
    }
    bool Land(Tank::Physics::TrackedVehicleTest& world)
    {
        for(int i=0;i<600;++i)
        {
            world.Step(dt);
            if(!world.State().mortarBlasts.empty())return true;
        }
        return false;
    }
}
int main()
{
    using namespace Tank::Physics;
    int failed=0;
    auto check=[&](bool ok,const char* message){if(!ok){std::cerr<<"FAIL "<<message<<'\n';++failed;}};
    TankSettings settings;
    settings.mortarMinimumFireAngleDegrees=0;
    settings.mortarMinimumRangeMeters=settings.mortarMaximumRangeMeters=15;
    settings.mortarMinimumAttackRadiusMeters=settings.mortarMaximumAttackRadiusMeters=4;
    TrackedVehicleTest world;
    world.Initialize(settings);Advance(world,60);
    world.AddFixedTurret({0,0.5f,15},{1,1,1},60);
    world.AddFixedTurret({0,0.5f,45},{1,1,1},60);
    check(Launch(world),"launch existing mortar");
    check(Land(world),"real impact");
    check(!world.State().fixedTurrets[0].target.active,"near turret destroyed");
    check(world.State().fixedTurrets[1].target.hitPoints==60,"outside turret unchanged");

    // Tuned damage and blast distance use the existing mortar settings.
    settings.mortarExplosionDamage=20;
    world.Initialize(settings);Advance(world,60);
    world.AddFixedTurret({0,0.5f,15},{1,1,1},100);
    // Rotated elongated proxy: compute the expected damage against its OBB.
    const float half=std::sqrt(0.5f);
    world.AddFixedTurret({3,0.5f,15},{6,1,0.5f},100,{0,half,0,half});
    check(Launch(world) && Land(world),"tuned damage landing");
    check(std::abs(world.State().fixedTurrets[0].target.hitPoints-80)<0.01f,"configured damage applies");
    const auto center=world.State().mortarBlasts.front().position;
    const float x=center.x-3,y=center.y-0.5f,z=center.z-15;
    const float dx=std::max(std::abs(z)-3,0.0f),dy=std::max(std::abs(y)-0.5f,0.0f),dz=std::max(std::abs(x)-0.25f,0.0f);
    const float distance=std::sqrt(dx*dx+dy*dy+dz*dz);
    const float expected=distance>4 ? 100 : 100-20*std::max(1-distance/4,0.1f);
    check(std::abs(world.State().fixedTurrets[1].target.hitPoints-expected)<0.01f,"rotated box falloff");

    // Slow real enemy rounds survive until impact; only nearby rounds are intercepted.
    settings.mortarExplosionDamage=80;
    world.Initialize(settings);Advance(world,60);
    EnemyAttackType type;
    type.detectionRangeMeters=type.reachMeters=type.firingRangeMeters=100;
    type.projectileSpeedMetersPerSecond=0.1f;type.firingIntervalSeconds=0.1f;
    type.maximumYawSpeedDegreesPerSecond=360;
    check(world.ConfigureEnemyAttacks({type}),"configure enemy fire");
    EnemyUnitType unit;unit.attackMounts[0].localPosition={2,1,0};
    world.AddFixedTurret({0,0.5f,15},{1,1,1},60,{0,1,0,0},"near",unit);
    world.AddFixedTurret({0,0.5f,45},{1,1,1},60,{0,1,0,0},"far",unit);
    Advance(world,30);
    std::vector<std::uint64_t> nearIds,farIds;
    const auto nearOwner=world.State().fixedTurrets[0].target.id;
    for(const auto& bullet:world.State().enemyProjectiles)
        (bullet.ownerId==nearOwner ? nearIds : farIds).push_back(bullet.target.id);
    check(!nearIds.empty() && !farIds.empty(),"both enemies have flying rounds");
    check(Launch(world) && Land(world),"interception explosion");
    auto exists=[&](std::uint64_t id){return std::any_of(world.State().enemyProjectiles.begin(),world.State().enemyProjectiles.end(),
        [id](const auto& bullet){return bullet.target.id==id;});};
    check(std::none_of(nearIds.begin(),nearIds.end(),exists),"near enemy rounds intercepted");
    check(std::all_of(farIds.begin(),farIds.end(),exists),"outside rounds preserved");
    check(world.State().playerCombat.hitPoints==100,"blast does not damage player");
    // Losing a life clears a flying mortar, then permits a fresh shot after respawn.
    world.Initialize(settings);Advance(world,60);
    check(Launch(world),"launch before loss");
    type.projectileSpeedMetersPerSecond=100;
    world.ConfigureEnemyAttacks({type});
    unit.attackMounts[0].localPosition={};
    world.AddFixedTurret({0,1.5f,-10},{1,1,1},60,{},"loss",unit);
    for(int i=0;i<60 && world.State().playerCombat.phase==PlayerCombatPhase::Alive;++i)world.Step(dt);
    check(world.State().playerCombat.phase==PlayerCombatPhase::Lost,"enemy causes loss during mortar flight");
    check(world.State().mortarProjectiles.empty() && world.State().mortarBlasts.empty(),"loss clears mortar flight and effects");
    for(int i=0;i<65 && world.State().playerCombat.phase==PlayerCombatPhase::Lost;++i)world.Step(dt);
    world.SetInput({});Advance(world,60);
    check(Launch(world),"mortar sequence reset allows shot after respawn");
    if(!failed)std::cout<<"PASS MortarEnemy\n";
    return failed ? 1 : 0;
}
