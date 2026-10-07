#include "Physics/TrackedVehicleTest.h"

#include <cmath>
#include <iostream>

namespace
{
    constexpr float dt = 1.0f / 60.0f;
    bool Check(bool condition, const char* message)
    {
        if (!condition) std::cerr << "FAIL Mortar projectile: " << message << '\n';
        return condition;
    }
    void Advance(Tank::Physics::TrackedVehicleTest& world, int steps)
    {
        for (int i = 0; i < steps; ++i) world.Step(dt);
    }
}

int main()
{
    using namespace Tank::Physics;
    bool passed = true;
    TrackedVehicleTest world;
    TankSettings settings;
    // Deterministic aim: canFire immediately, fixed range and blast radius.
    settings.mortarMinimumFireAngleDegrees = 0.0f;
    settings.mortarMinimumRangeMeters = 15.0f;
    settings.mortarMaximumRangeMeters = 15.0f;
    settings.mortarMinimumAttackRadiusMeters = 4.0f;
    settings.mortarMaximumAttackRadiusMeters = 4.0f;
    world.Initialize(settings);
    Advance(world, 60);

    TankInput input;
    input.leftLeverX = -1.0f;
    input.rightLeverX = 1.0f;
    world.SetInput(input);
    Advance(world, 10);
    passed &= Check(world.State().specialMove.state == SpecialMoveState::MortarAiming,
        "gesture enters MortarAiming with canFire ready");
    passed &= Check(world.State().mortarAim.canFire, "aim is ready at minimum fire angle 0");

    // The fire button during aiming launches one shell and releases the wheelie.
    input.fireAssault = true;
    world.SetInput(input);
    world.Step(dt);
    passed &= Check(world.State().mortarProjectiles.size() == 1, "fire spawns one mortar shell");
    passed &= Check(world.State().mortarShotsFired == 1, "shot counter increments");
    passed &= Check(world.State().specialMove.state == SpecialMoveState::Idle,
        "firing releases the wheelie immediately");
    // Holding the same button press after the wheelie releases must not leak
    // into assault automatic fire. Track the flight apex during the hold too.
    const float launchY = world.State().mortarProjectiles.front().position.y;
    float apexY = launchY;
    for (int i = 0; i < 30; ++i)
    {
        world.Step(dt);
        if (!world.State().mortarProjectiles.empty())
            apexY = (std::max)(apexY, world.State().mortarProjectiles.front().position.y);
    }
    passed &= Check(world.State().assaultWeapon.roundsFired == 0,
        "held fire button after mortar launch does not fire assault rounds");
    input.fireAssault = false;
    world.SetInput(input);

    // Parabolic flight: the shell must climb above its launch point.
    bool blastObserved = false;
    Vec3 blastPosition = {};
    int steps = 0;
    while (!world.State().mortarProjectiles.empty() && steps < 600)
    {
        world.Step(dt);
        ++steps;
        if (!world.State().mortarProjectiles.empty())
            apexY = (std::max)(apexY, world.State().mortarProjectiles.front().position.y);
        if (!world.State().mortarBlasts.empty())
        {
            blastObserved = true;
            blastPosition = world.State().mortarBlasts.front().position;
        }
    }
    passed &= Check(apexY > launchY + 0.1f, "shell follows a parabola above the muzzle");
    passed &= Check(world.State().mortarProjectiles.empty(), "shell is removed after impact");
    passed &= Check(blastObserved, "impact creates a blast effect");
    passed &= Check(std::abs(blastPosition.x) < 2.0f &&
        std::abs(blastPosition.z - 15.0f) < 2.0f, "blast lands near the cue center");
    const int autoFlightSteps = steps + 30; // Include the 30-step hold above.

    // Spherical damage: re-arm and place boxes at the expected impact point.
    world.Initialize(settings);
    Advance(world, 60);
    passed &= Check(world.AddDestructibleBox({0.0f, 0.5f, 15.0f}, {1.0f, 1.0f, 1.0f}, 60.0f),
        "create near box");
    passed &= Check(world.AddDestructibleBox({0.0f, 0.5f, 45.0f}, {1.0f, 1.0f, 1.0f}, 60.0f),
        "create far box");
    world.SetInput(input);
    Advance(world, 10);
    input.fireAssault = true;
    world.SetInput(input);
    world.Step(dt);
    input.fireAssault = false;
    world.SetInput(input);
    steps = 0;
    while (!world.State().mortarProjectiles.empty() && steps < 600)
    {
        world.Step(dt);
        ++steps;
    }
    passed &= Check(!world.State().destructibleBoxes[0].target.active,
        "near box is destroyed by the spherical blast");
    passed &= Check(world.State().destructibleBoxes[1].target.hitPoints == 60.0f,
        "far box outside the blast radius is untouched");

    // One shell per gesture: holding the levers after firing must not re-trigger.
    Advance(world, 120);
    passed &= Check(world.State().specialMove.state == SpecialMoveState::Idle &&
        world.State().mortarShotsFired == 1, "held gesture does not re-arm the mortar");

    // Emergency brake: a mortar gesture while driving is accepted and the
    // tank brakes to a stop before aiming begins.
    world.Initialize(settings);
    Advance(world, 60);
    TankInput drive;
    drive.throttle = 1.0f;
    world.SetInput(drive);
    Advance(world, 120);
    passed &= Check(world.State().speedMetersPerSecond > 2.0f, "tank is driving before the gesture");
    passed &= Check(world.State().mobility.state == MobilityState::Moving,
        "mobility is Moving while driving");
    TankInput gesture = drive;
    gesture.throttle = 0.0f;
    gesture.leftLeverX = -1.0f;
    gesture.rightLeverX = 1.0f;
    world.SetInput(gesture);
    world.Step(dt);
    passed &= Check(world.State().specialMove.state == SpecialMoveState::MortarStarting,
        "mortar gesture accepted while moving");
    passed &= Check(world.State().mortarAim.angleDegrees == 0.0f,
        "stance stays down during the braking phase");
    int brakeSteps = 0;
    while (world.State().specialMove.state == SpecialMoveState::MortarStarting && brakeSteps < 300)
    {
        world.Step(dt);
        ++brakeSteps;
    }
    passed &= Check(world.State().specialMove.state == SpecialMoveState::MortarAiming,
        "emergency brake reaches mortar aiming without a second gesture");
    passed &= Check(brakeSteps < 150, "emergency brake stops the tank within 2.5 s");
    gesture.fireAssault = true;
    world.SetInput(gesture);
    world.Step(dt);
    passed &= Check(world.State().mortarShotsFired == 1, "mortar fires after the emergency brake");

    // Fixed muzzle velocity time-scales the same trajectory: identical arc,
    // shorter flight time, same landing point.
    TankSettings fixedSpeedSettings = settings;
    fixedSpeedSettings.mortarMuzzleVelocityAuto = false;
    fixedSpeedSettings.mortarMuzzleVelocityMetersPerSecond = 25.0f;
    world.Initialize(fixedSpeedSettings);
    Advance(world, 60);
    world.SetInput(input);
    Advance(world, 10);
    input.fireAssault = true;
    world.SetInput(input);
    world.Step(dt);
    input.fireAssault = false;
    world.SetInput(input);
    steps = 0;
    blastObserved = false;
    while (!world.State().mortarProjectiles.empty() && steps < 900)
    {
        world.Step(dt);
        ++steps;
        if (!world.State().mortarBlasts.empty())
        {
            blastObserved = true;
            blastPosition = world.State().mortarBlasts.front().position;
        }
    }
    passed &= Check(blastObserved &&
        std::abs(blastPosition.x) < 2.0f && std::abs(blastPosition.z - 15.0f) < 2.0f,
        "fixed muzzle velocity still lands on the cue center");
    passed &= Check(steps < autoFlightSteps,
        "higher muzzle velocity reaches the cue faster");

    // A slower muzzle velocity flies the same arc but takes longer.
    TankSettings slowSettings = settings;
    slowSettings.mortarMuzzleVelocityAuto = false;
    slowSettings.mortarMuzzleVelocityMetersPerSecond = 5.0f;
    world.Initialize(slowSettings);
    Advance(world, 60);
    world.SetInput(input);
    Advance(world, 10);
    input.fireAssault = true;
    world.SetInput(input);
    world.Step(dt);
    input.fireAssault = false;
    world.SetInput(input);
    steps = 0;
    blastObserved = false;
    while (!world.State().mortarProjectiles.empty() && steps < 900)
    {
        world.Step(dt);
        ++steps;
        if (!world.State().mortarBlasts.empty())
        {
            blastObserved = true;
            blastPosition = world.State().mortarBlasts.front().position;
        }
    }
    passed &= Check(blastObserved &&
        std::abs(blastPosition.x) < 2.0f && std::abs(blastPosition.z - 15.0f) < 2.0f,
        "slow muzzle velocity still lands on the cue center");
    passed &= Check(steps > autoFlightSteps,
        "lower muzzle velocity takes longer to reach the cue");

    // Wheelie stance locks drive input: throttle during MortarAiming must
    // not move the tank.
    world.Initialize(settings);
    Advance(world, 60);
    world.SetInput(input);
    Advance(world, 10);
    passed &= Check(world.State().specialMove.state == SpecialMoveState::MortarAiming,
        "gesture enters MortarAiming for the drive lock check");
    TankInput locked = input;
    locked.throttle = 1.0f;
    const Vec3 stancePosition = world.State().bodyPosition;
    world.SetInput(locked);
    Advance(world, 60);
    passed &= Check(world.State().specialMove.state == SpecialMoveState::MortarAiming,
        "throttle during the wheelie does not cancel the stance");
    const Vec3& drifted = world.State().bodyPosition;
    const float driftXZ = std::sqrt(
        (drifted.x - stancePosition.x) * (drifted.x - stancePosition.x) +
        (drifted.z - stancePosition.z) * (drifted.z - stancePosition.z));
    passed &= Check(driftXZ < 0.5f,
        "throttle during the wheelie does not move the tank");

    if (!passed) return 1;
    std::cout << "PASS MortarProjectile\n";
    return 0;
}
