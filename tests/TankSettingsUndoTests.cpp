#include "App/TankSettingsUndo.h"
#include "Input/InputDeviceProfiles.h"
#include "Input/TankInputMapper.h"
#include "Physics/PhysicsEnvironmentSettings.h"
#include "Physics/EnemyAttackType.h"
#include "Rendering/TankVisualSettings.h"

#include <iostream>
#include <stdexcept>

namespace
{
    void Check(bool condition, const char* message)
    {
        if (!condition) throw std::runtime_error(message);
    }
}

int main()
{
    Tank::Physics::TankSettings settings;
    const auto initial = settings;
    auto appliedSettings = settings;
    Tank::App::TankSettingsUndo undo(settings);
    auto change = [&](auto mutate, unsigned interaction)
    {
        const auto before = settings;
        mutate(settings);
        undo.Observe(before, interaction);
    };

    Check(!undo.Undo() && !undo.Redo(), "An empty history must be harmless");
    change([](auto& value) { value.chassisMassKg = 4500; }, 101);
    change([](auto& value) { value.chassisMassKg = 5000; }, 101);
    Check(undo.CanUndo() && undo.UndoCount() == 0, "An active drag must remain one pending edit");
    undo.FinishInteraction(0);
    Check(undo.UndoCount() == 1, "Releasing the mass slider must record one edit");
    appliedSettings = settings; // Represents an explicit Apply & Reset.
    Check(undo.Undo() && settings == initial, "Mass undo must restore the initial edited settings");
    Check(appliedSettings.chassisMassKg == 5000, "Undo must leave explicitly applied physics settings alone");
    Check(undo.Redo() && settings.chassisMassKg == 5000, "Mass redo must restore the final edited value");

    change([](auto& value) { value.engineMaxTorqueNm = 1200; }, 102);
    change([](auto& value) { value.suspensionStrokeMeters.back() = 0.35f; }, 103);
    Check(undo.UndoCount() == 2, "Switching active controls must finish the previous edit");
    undo.Undo(); // Must flush the suspension drag before undoing it.
    Check(settings.suspensionStrokeMeters == initial.suspensionStrokeMeters &&
        settings.engineMaxTorqueNm == 1200, "Suspension undo must preserve the preceding engine edit");
    undo.Undo();
    Check(settings.engineMaxTorqueNm == initial.engineMaxTorqueNm,
        "Engine edits must be independently undoable");

    const auto baseline = settings;
    // A profile load or optimizer result changes several fields at once.
    change([](auto& value)
    {
        value.rollSpeedMultiplier = 2;
        value.rollSpeedTuning.driveTorque = 6;
        value.mortarExplosionDamage = 100;
        value.assaultProjectiles.maximumCount = 128;
        value.assaultProjectiles.muzzleLocalPosition = {1, 2, 3};
        value.forwardGearRatios[0] = 5;
        value.startUpsideDown = true;
    }, 0);
    const auto profile = settings;
    Check(!undo.CanRedo(), "A new edit must discard the prior redo branch");
    undo.Undo();
    Check(settings == baseline, "A batch must restore nested, array, numeric and boolean fields together");
    undo.Redo();
    Check(settings == profile, "A batch redo must reproduce the complete profile");

    undo.Undo();
    const auto count = undo.UndoCount();
    change([](auto& value) { value.rollSpeedMultiplier = 1.5f; }, 104);
    change([&](auto& value) { value = baseline; }, 104);
    undo.Flush();
    Check(undo.UndoCount() == count && undo.CanRedo(), "A drag returning to its start must preserve redo");
    undo.Redo();
    Check(settings == profile, "The retained redo must still restore the profile");

    undo.Flush(); // Repeated finalization must not add an empty command.
    Check(undo.UndoCount() == count + 1, "Flushing must preserve existing commands");
    change([](auto& value) { value.trackWidthM = 0.6f; }, 105);
    undo.Clear();
    undo.FinishInteraction(0);
    Check(!undo.CanUndo() && !undo.CanRedo(), "Leaving a session must clear pending and committed edits");
    Check(settings.trackWidthM == 0.6f, "Clearing history must preserve the edited values");
    Tank::Rendering::TankVisualSettings visual;
    Tank::Physics::PhysicsEnvironmentSettings environment;
    Tank::Physics::EnemyEditorSettings enemies;
    const auto visualInitial = visual;
    const auto environmentInitial = environment;
    const auto enemiesInitial = enemies;
    int visualApplications = 0;
    visual.hullUpper.albedo.r = 0.9f;
    undo.ObserveExternal(visualInitial, visual, 201,
        [&](const auto& value) { visual = value; ++visualApplications; }, "Visual");
    auto previousVisual = visual;
    visual.gltfModelScale = 2.0f;
    undo.ObserveExternal(previousVisual, visual, 201,
        [&](const auto& value) { visual = value; ++visualApplications; }, "Visual");
    undo.FinishInteraction(202);
    environment.floorFriction = 0.2f;
    environment.obstacleSeed = 42;
    undo.ObserveExternal(environmentInitial, environment, 202,
        [&](const auto& value) { environment = value; }, "Environment");
    undo.FinishInteraction(203);
    enemies = enemiesInitial;
    // A catalog edit may replace the complete unit/attack list.
    enemies.unitTypes.clear();
    enemies.unitTypes.emplace_back();
    enemies.unitTypes.back().name = "Undo test";
    undo.ObserveExternal(enemiesInitial, enemies, 203,
        [&](const auto& value) { enemies = value; }, "Enemies");
    change([](auto& value) { value.chassisMassKg = 7000; }, 204);
    Check(undo.Undo() && settings.chassisMassKg != 7000, "Tank edits must follow external edits");
    Check(undo.Undo() && enemies == enemiesInitial, "Enemy catalog undo must restore the complete catalog");
    Check(undo.Undo() && environment == environmentInitial, "Environment undo must restore all edited values");
    Check(undo.Undo() && visual == visualInitial && visualApplications == 1,
        "Visual drag must be one command and refresh its presentation");
    Check(undo.Redo() && visual.gltfModelScale == 2.0f && visualApplications == 2,
        "Visual redo must restore and refresh the final appearance");
    undo.Clear();
    const auto savedVisual = visual;
    visual.gltfModelScale = 3.0f;
    undo.ObserveExternal(savedVisual, visual, 301,
        [&](const auto& value) { visual = value; }, "Visual");
    previousVisual = visual;
    visual = savedVisual;
    undo.ObserveExternal(previousVisual, visual, 301,
        [&](const auto& value) { visual = value; }, "Visual");
    undo.FinishInteraction(0);
    Check(!undo.CanUndo(), "An external drag returning to its origin must add no command");
    environment.floorSizeM += 1;
    undo.ObserveExternal(environmentInitial, environment, 302,
        [&](const auto& value) { environment = value; }, "Environment");
    undo.Clear();
    undo.FinishInteraction(0);
    Check(!undo.CanUndo(), "Session cleanup must discard pending external edits");
    Tank::Input::InputDeviceProfiles devices;
    devices.profiles.emplace_back();
    devices.profiles.front().id = "pad-A";
    devices.profiles.emplace_back();
    devices.profiles.back().id = "pad-B";
    const auto devicesInitial = devices;
    auto previousDevices = devices;
    devices.profiles.front().deadzone = 0.25f;
    undo.ObserveExternal(previousDevices, devices, 401,
        [&](const auto& value) { devices = value; }, "Input devices");
    previousDevices = devices;
    devices.profiles.front().deadzone = 0.3f;
    undo.ObserveExternal(previousDevices, devices, 401,
        [&](const auto& value) { devices = value; }, "Input devices");
    undo.FinishInteraction(402);
    previousDevices = devices;
    devices.profiles.back().leftTrackAxis = 8;
    devices.profiles.back().fireButton = 9;
    devices.profiles.back().invertLeftTrack = false;
    devices.profiles.back().neutral = 0.4f;
    undo.ObserveExternal(previousDevices, devices, 402,
        [&](const auto& value) { devices = value; }, "Input devices");
    Check(undo.Undo() && devices.profiles.back() == devicesInitial.profiles.back() &&
        devices.profiles.front().deadzone == 0.3f, "Device edits must restore the correct device across selection changes");
    Check(undo.Undo() && devices == devicesInitial, "Deadzone drag must undo as one command");
    undo.Redo();
    const auto beforeLoad = devices;
    devices.profiles.clear();
    undo.ObserveExternal(beforeLoad, devices, 0,
        [&](const auto& value) { devices = value; }, "Load devices");
    Check(undo.Undo() && devices == beforeLoad, "Loading profiles must be undoable despite vector replacement");
    Tank::Input::TankInputMappingSettings mapping;
    const auto mappingInitial = mapping;
    mapping.stickDeadzone = 0.4f;
    mapping.leftLeverAxis = 7;
    undo.ObserveExternal(mappingInitial, mapping, 0,
        [&](const auto& value) { mapping = value; }, "Mapping");
    Check(undo.Undo() && mapping == mappingInitial, "Mapping undo must restore axis and deadzone values");
    undo.Clear();
    std::cout << "Tank settings undo tests passed\n";
    return 0;
}
