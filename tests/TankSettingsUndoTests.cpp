#include "App/TankSettingsUndo.h"

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
    std::cout << "Tank settings undo tests passed\n";
    return 0;
}
