#include "App/UndoHistory.h"
#include "App/ExternalUndoEdit.h"
#include "App/CameraEditState.h"
#include <tuple>
#include "App/SnapshotUndoEdit.h"
#include "Physics/EnemyAttackType.h"

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
    int value = 0;
    Tank::App::UndoHistory history;
    history.Execute(std::make_unique<Tank::App::LambdaUndoCommand>(
        "Increment", [&] { value -= 1; }, [&] { value += 1; }));
    Check(value == 1 && history.UndoCount() == 1, "Execute must apply and record a command");
    Check(history.Undo() == "Increment" && value == 0, "Undo must restore the prior value");
    Check(history.Redo() == "Increment" && value == 1, "Redo must apply the value again");

    history.Undo();
    history.Execute(std::make_unique<Tank::App::LambdaUndoCommand>(
        "Double", [&] { value /= 2; }, [&] { value *= 2; }));
    Check(value == 0 && !history.CanRedo(), "A new command must discard the redo branch");

    for (int i = 0; i < 10000; ++i)
    {
        history.Execute(std::make_unique<Tank::App::LambdaUndoCommand>(
            "Add", [&] { value -= 1; }, [&] { value += 1; }));
    }
    Check(history.UndoCount() == 10001, "History must have no fixed command count limit");
    while (history.CanUndo()) history.Undo();
    Check(value == 0 && history.RedoCount() == 10001, "All commands must be undoable");
    while (history.CanRedo()) history.Redo();
    Check(value == 10000 && history.UndoCount() == 10001, "All commands must be redoable");

    history.Clear();
    Check(!history.CanUndo() && !history.CanRedo(), "Clear must reset both history directions");

    Tank::Physics::EnemyEditorSettings catalog;
    Tank::App::SnapshotUndoEdit<Tank::Physics::EnemyEditorSettings> edit;
    const auto original = catalog;
    const auto apply = [&](const auto& settings) { catalog = settings; };
    auto change = [&](auto mutate, unsigned interaction)
    {
        const auto before = catalog;
        mutate(catalog);
        edit.Update(before, catalog, interaction, history, apply, "Enemy catalog edit");
    };
    change([](auto& settings) { settings.unitTypes[0].hitPoints = 100; }, 11);
    change([](auto& settings) { settings.unitTypes[0].hitPoints = 150; }, 11);
    Check(!history.CanUndo() && edit.HasPending(), "A drag must stay pending across frames");
    edit.Update(catalog, catalog, 0, history, apply, "Enemy catalog edit");
    Check(history.UndoCount() == 1, "A whole drag must be one command");
    history.Undo();
    Check(catalog == original, "Enemy HP undo must restore the initial settings");
    history.Redo();
    Check(catalog.unitTypes[0].hitPoints == 150, "Enemy HP redo must restore the final settings");

    change([](auto& settings) { settings.unitTypes[0].name.clear(); }, 22);
    // Switching directly between two active widgets must split their edits.
    change([](auto& settings) { settings.simulation.nodeArrivalMeters = 1.5f; }, 23);
    Check(history.UndoCount() == 2 && catalog.simulation.nodeArrivalMeters == 1.5f,
        "Recording the prior interaction must not replay it over the next edit");
    edit.Flush(history, apply, "Enemy catalog edit");
    history.Undo();
    Check(catalog.unitTypes[0].name.empty() && catalog.simulation.nodeArrivalMeters == 0.5f,
        "Undo must support temporarily invalid catalog data");
    history.Undo();
    Check(catalog.unitTypes[0].name == original.unitTypes[0].name,
        "Name changes must be independently undoable");

    const auto beforeNoOp = history.UndoCount();
    const auto current = catalog;
    change([](auto& settings) { settings.unitTypes[0].hitPoints += 10; }, 33);
    change([&](auto& settings) { settings = current; }, 33);
    edit.Flush(history, apply, "Enemy catalog edit");
    Check(history.UndoCount() == beforeNoOp && history.CanRedo(),
        "Returning to the original value must preserve the redo branch");

    change([](auto& settings)
    {
        settings.attackTypes[0].projectileSpeedMetersPerSecond = 25;
        settings.unitTypes[0].attackMounts.push_back({{1, 2, 3}, 0});
        settings.unitTypes.push_back({});
    }, 0);
    const auto changedCatalog = catalog;
    int mapPosition = 0;
    history.Execute(std::make_unique<Tank::App::LambdaUndoCommand>("Map edit",
        [&] { mapPosition = 0; }, [&] { mapPosition = 10; }));
    history.Undo();
    Check(mapPosition == 0 && catalog == changedCatalog,
        "A shared history must undo the latest map edit first");
    history.Undo();
    Check(catalog == current, "Attack, mount and unit additions must restore together");
    history.Redo();
    Check(catalog == changedCatalog, "Catalog redo must restore nested settings");
    change([](auto& settings) { settings.unitTypes[0].hitPoints = 75; }, 44);
    edit.Clear();
    history.Clear();
    Check(!edit.HasPending() && !history.CanUndo(), "A new session must clear pending edits and history");
    Tank::App::ExternalUndoEdit viewEdit;
    Tank::App::CameraEditState camera;
    const auto initialCamera = camera;
    auto previousCamera = camera;
    camera.settings.position[0] = 8;
    camera.settings.fovDegrees = 70;
    viewEdit.Update(previousCamera, camera, 51, history,
        [&](const auto& value) { camera = value; }, "Camera");
    previousCamera = camera;
    camera.settings.position[0] = 9;
    viewEdit.Update(previousCamera, camera, 51, history,
        [&](const auto& value) { camera = value; }, "Camera");
    viewEdit.FinishInteraction(52);
    auto preview = std::make_tuple(true, 1.0f, 10, false);
    const auto initialPreview = preview;
    std::get<0>(preview) = false;
    std::get<1>(preview) = 2.0f;
    viewEdit.Update(initialPreview, preview, 52, history,
        [&](const auto& value) { preview = value; }, "Grid");
    viewEdit.Flush();
    history.Execute(std::make_unique<Tank::App::LambdaUndoCommand>("Object",
        [&] { mapPosition = 0; }, [&] { mapPosition = 20; }));
    history.Undo();
    Check(mapPosition == 0 && !std::get<0>(preview), "Object undo must preserve preview edits");
    history.Undo();
    Check(preview == initialPreview && camera.settings.position[0] == 9,
        "Grid undo must preserve the preceding camera edit");
    history.Undo();
    Check(camera == initialCamera, "Camera drag must restore pose and projection together");
    history.Redo();
    Check(camera.settings.position[0] == 9 && camera.settings.fovDegrees == 70,
        "Camera redo must restore the final drag values");
    previousCamera = camera;
    camera.nearZ = 0.5f; camera.farZ = 2000; camera.mouseSpeed = 3;
    viewEdit.Update(previousCamera, camera, 53, history,
        [&](const auto& value) { camera = value; }, "Camera clipping");
    viewEdit.Flush(); history.Undo();
    Check(camera == previousCamera, "Clipping and mouse speed must be undoable");
    history.Clear();
    previousCamera = camera;
    camera.settings.followDistance += 1;
    viewEdit.Update(previousCamera, camera, 54, history,
        [&](const auto& value) { camera = value; }, "Camera follow");
    viewEdit.Clear(); viewEdit.Flush();
    Check(!history.CanUndo(), "Session cleanup must discard pending view edits");
    history.Clear();
    edit.Clear();
    catalog.simulation.movementEnabled = true;
    catalog.simulation.nodeArrivalMeters = 1.25f;
    const auto beforeMovementToggle = catalog;
    change([](auto& settings) { settings.simulation.movementEnabled = false; }, 0);
    Check(history.UndoCount() == 1 && !catalog.simulation.movementEnabled,
        "Movement checkbox records one command");
    history.Undo();
    Check(catalog == beforeMovementToggle, "Movement Undo restores ON and retained tuning");
    history.Redo();
    Check(!catalog.simulation.movementEnabled && catalog.simulation.nodeArrivalMeters == 1.25f,
        "Movement Redo restores OFF without discarding hidden tuning");
    std::cout << "Undo history tests passed\n";
    return 0;
}
