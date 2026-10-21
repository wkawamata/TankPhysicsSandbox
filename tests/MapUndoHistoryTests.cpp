#include "App/ManifestUndoEdit.h"
#include "Map/MapFolder.h"
#include "Map/MapHierarchy.h"

#define NOMINMAX
#include <Windows.h>
#include <chrono>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>

namespace
{
    void Check(bool condition, const char* message)
    {
        if (!condition) throw std::runtime_error(message);
    }

    struct TestFolder
    {
        std::filesystem::path path = std::filesystem::temp_directory_path() /
            ("TankMapUndoTests-" + std::to_string(GetCurrentProcessId()) + "-" +
                std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        TestFolder() { Check(std::filesystem::create_directory(path), "Create test folder"); }
        ~TestFolder() { std::error_code error; std::filesystem::remove_all(path, error); }
    };

    std::string Read(const std::filesystem::path& path)
    {
        std::ifstream input(path, std::ios::binary);
        Check(static_cast<bool>(input), "Read saved manifest");
        return std::string(std::istreambuf_iterator<char>(input), {});
    }
}

int RunTests()
{
    using namespace Tank::Map;
    TestFolder folder;
    MapFolder map;
    std::string error;
    if (!map.Open(folder.path, error)) throw std::runtime_error("Open editing session: " + error);
    const auto initial = map.Document();
    const auto path = folder.path / "Manifest.json";
    const auto initialFile = Read(path);
    Tank::App::UndoHistory history;
    Tank::App::ManifestUndoEdit edit;
    const auto apply = [&](const Manifest& state)
    {
        Check(map.SetManifest(state, error), "Apply an undo snapshot");
    };
    auto change = [&](auto mutate, unsigned interaction)
    {
        const auto before = map.Document();
        auto after = before;
        mutate(after);
        Check(map.SetManifest(after, error), "Apply a map edit");
        edit.Update(before, after, interaction, history, apply, "Map edit");
    };

    change([](auto& state) { state.playerSpawn.position[0] = 1; }, 11);
    change([](auto& state) { state.playerSpawn.position[0] = 2; }, 11);
    Check(history.UndoCount() == 0 && edit.HasPending(), "Continuous edits remain pending");
    change([](auto& state) { state.playerSpawn.position[1] = 3; }, 22);
    Check(history.UndoCount() == 1 && map.Document().playerSpawn.position[1] == 3,
        "Switching active fields records the last snapshot without replaying over a new edit");
    edit.Flush(history, apply, "Map edit");
    Check(history.UndoCount() == 2, "Distinct field interactions must be separate commands");
    history.Undo();
    Check(map.Document().playerSpawn.position[0] == 2 && map.Document().playerSpawn.position[1] == 2,
        "Undo restores only the last interaction");
    history.Undo();
    Check(!map.IsDirty() && Tank::App::ManifestUndoEqual{}(map.Document(), initial),
        "Undoing all edits must restore the saved state and clear dirty status");
    Check(Read(path) == initialFile, "Undo must not overwrite the saved manifest");
    history.Redo();
    Check(map.Save(error) && !map.IsDirty(), "Save records a new saved baseline");
    const auto savedFile = Read(path);
    history.Undo();
    Check(map.IsDirty() && Read(path) == savedFile,
        "Undo after Save changes memory and dirty state, while retaining the saved file");
    history.Redo();
    Check(!map.IsDirty(), "Redoing the saved snapshot must clear dirty status again");

    change([](auto& state)
    {
        state.nodes.push_back({"group", "Group", "root", {}});
        state.nodes.push_back({"child", "Child", "group", {}});
    }, 0);
    const auto hierarchy = map.Document();
    change([&](auto& state) { Check(MoveNode(state, "child", "root", NodePlacement::Child, error), "Reparent"); }, 0);
    history.Undo();
    Check(Tank::App::ManifestUndoEqual{}(map.Document(), hierarchy), "Undo restores parent and sibling ordering");
    change([&](auto& state) { Check(DuplicateNode(state, "group", error).has_value(), "Duplicate subtree"); }, 0);
    Check(!history.CanRedo(), "A new hierarchy edit must discard the old redo branch");
    history.Undo();
    Check(Tank::App::ManifestUndoEqual{}(map.Document(), hierarchy), "Undo removes the entire duplicated subtree");
    change([&](auto& state) { Check(RemoveNode(state, "child", error), "Remove leaf node"); }, 0);
    history.Undo();
    Check(Tank::App::ManifestUndoEqual{}(map.Document(), hierarchy), "Undo restores a removed node and its parent");

    const auto count = history.UndoCount();
    change([](auto& state) { state.playerSpawn.position[0] += 5; }, 33);
    change([&](auto& state) { state = hierarchy; }, 33);
    edit.Flush(history, apply, "Map edit");
    Check(history.UndoCount() == count && history.CanRedo(), "A net-zero interaction preserves redo history");

    change([](auto& state) { state.playerSpawn.position[2] = 5; }, 44);
    edit.Clear();
    history.Clear();
    map = {};
    Check(map.Open(folder.path, error), "Reopen map in a new session");
    edit.FinishInteraction(0, history, apply, "Map edit");
    Check(!history.CanUndo() && !edit.HasPending(), "Closed sessions must not leak pending or saved commands");
    const auto savedName = map.Document().name;
    const auto fileBeforeRename = Read(path);
    change([](auto& state) { state.name = "Renamed arena"; }, 0);
    Check(map.Document().name == "Renamed arena" && map.IsDirty(), "Rename changes the map document");
    history.Undo();
    Check(map.Document().name == savedName && !map.IsDirty(), "Undo restores saved map name");
    history.Redo();
    Check(map.Document().name == "Renamed arena", "Redo restores map name");
    Check(Read(path) == fileBeforeRename, "Undo/Redo rename leaves disk unchanged until Save");
    Check(map.Save(error), "Save renamed map");
    MapFolder reopened;
    Check(reopened.Open(folder.path, error) && reopened.Document().name == "Renamed arena", "Saved name survives reopen");
    std::cout << "Map undo history tests passed\n";
    return 0;
}

int main()
{
    try { return RunTests(); }
    catch (const std::exception& exception)
    {
        std::cerr << exception.what() << '\n';
        return 1;
    }
}
