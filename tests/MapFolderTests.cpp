#include "Map/MapFolder.h"

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
            ("TankMapFolderTests-" + std::to_string(GetCurrentProcessId()) + "-" +
                std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        TestFolder() { Check(std::filesystem::create_directory(path), "Create isolated test folder"); }
        ~TestFolder() { std::error_code error; std::filesystem::remove_all(path, error); }
    };

    std::string Read(const std::filesystem::path& path)
    {
        std::ifstream input(path, std::ios::binary);
        Check(static_cast<bool>(input), "Read saved file");
        return std::string(std::istreambuf_iterator<char>(input), {});
    }

    struct LockedFile
    {
        HANDLE handle;
        explicit LockedFile(const std::filesystem::path& path) : handle(CreateFileW(path.c_str(),
            GENERIC_READ, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr))
        { Check(handle != INVALID_HANDLE_VALUE, "Lock manifest for failure test"); }
        ~LockedFile() { CloseHandle(handle); }
    };
}

int main()
{
    using namespace Tank::Map;
    try
    {
        TestFolder root;
        const auto folder = root.path / L"Map \u30de\u30c3\u30d7";
        std::filesystem::create_directory(folder);
        const auto path = folder / "Manifest.json";
        std::string error;
        MapFolder session;
        Check(!session.Save(error) && !error.empty(), "Cannot save unopened session");
        Check(!session.SetManifest({}, error), "Cannot edit unopened session");
        Check(!session.Open({}, error) && !session.IsOpen(), "Empty folder path rejected");
        Check(!session.Open(root.path / "missing", error), "Missing folder rejected");
        Check(session.Open(folder, error), "Create new manifest in Unicode folder");
        Check(session.IsOpen() && !session.IsDirty() && error.empty(), "New session is saved");
        Check(std::filesystem::is_regular_file(path), "Manifest.json exists");
        const std::string original = Read(path);
        for (size_t i = 0; i < original.size(); ++i)
            Check(original[i] != '\n' || (i > 0 && original[i - 1] == '\r'), "Saved JSON uses CRLF");

        auto changed = session.Document();
        changed.playerSpawn.position = { 10, 3, 20 };
        changed.playerSpawn.rotationDegrees = { 5, 90, 10 };
        changed.instances.push_back({ "model-1", "Models/House.glb", { { 2, 3, 4 }, { 5, 6, 7 } } });
        changed.clearAreas.push_back({ "goal-1", "Goal", { 3, 4, 5 }, { 6, 7, 8 } });
        Check(session.SetManifest(changed, error) && session.IsDirty(), "Editing marks dirty");
        Check(Read(path) == original, "Editing does not auto-save");
        Check(session.Save(error) && !session.IsDirty(), "Save clears dirty state");
        const auto saved = Read(path);
        MapFolder reopened;
        Check(reopened.Open(folder, error) && !reopened.IsDirty(), "Reopen existing manifest");
        Check(Read(path) == saved, "Opening existing manifest does not rewrite it");
        std::string expected, actual;
        Check(SerializeManifest(changed, expected, error) &&
            SerializeManifest(reopened.Document(), actual, error) && expected == actual,
            "Every field survives file round trip");

        changed.playerSpawn.position[0] = 25;
        Check(session.SetManifest(changed, error) && session.IsDirty(), "Edit before discard");
        session.DiscardChanges();
        Check(!session.IsDirty() && session.Document().playerSpawn.position[0] == 10,
            "Discard restores the last saved manifest");

        const auto badFolder = root.path / "bad";
        std::filesystem::create_directory(badFolder);
        { std::ofstream output(badFolder / "Manifest.json"); output << "invalid json"; }
        changed.playerSpawn.position[0] = 25;
        Check(session.SetManifest(changed, error), "Edit before failed open");
        Check(!session.Open(badFolder, error) && !error.empty() && session.Folder() == std::filesystem::canonical(folder) &&
            session.IsDirty() && session.Document().playerSpawn.position[0] == 25,
            "Failed open preserves folder and unsaved document");
        Check(Read(badFolder / "Manifest.json") == "invalid json", "Invalid manifest is not replaced");

        const auto wrongType = root.path / "directory-manifest";
        std::filesystem::create_directories(wrongType / "Manifest.json");
        Check(!session.Open(wrongType, error), "Directory named Manifest.json rejected");
        {
            LockedFile locked(path);
            Check(!session.Save(error) && !error.empty() && session.IsDirty(), "Save failure keeps unsaved edits");
            Check(!reopened.Open(folder, error), "Read failure reported");
        }
        Check(Read(path) == saved, "Save failure preserves the entire original file");
        Check(std::distance(std::filesystem::directory_iterator(folder), std::filesystem::directory_iterator{}) == 1,
            "Failed save cleans up temporary file");
        Check(session.Save(error) && !session.IsDirty(), "Save can be retried after failure");
        changed.clearAreas[0].size[0] = -1;
        Check(!session.SetManifest(changed, error) && !session.IsDirty(), "Invalid edit rejected without dirtying session");
        const auto second = root.path / "second";
        std::filesystem::create_directory(second);
        Check(session.Open(second, error) && session.Document().instances.empty() && !session.IsDirty(),
            "Switching folders replaces previous document");
        std::cout << "PASS MapFolder\n";
        return 0;
    }
    catch (const std::exception& exception)
    {
        std::cerr << "FAIL MapFolder: " << exception.what() << '\n';
        return 1;
    }
}
