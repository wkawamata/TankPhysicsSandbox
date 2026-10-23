#include "Map/MapCatalog.h"
#include "Map/MapFolder.h"

#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace
{
    void Check(bool condition, const char* message)
    {
        if (!condition) throw std::runtime_error(message);
    }

    struct TestRoot
    {
        std::filesystem::path path = std::filesystem::temp_directory_path() /
            ("TankMapCatalog-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        TestRoot() { std::filesystem::create_directory(path); }
        ~TestRoot() { std::error_code error; std::filesystem::remove_all(path, error); }
    };

    void WriteMap(const std::filesystem::path& folder, const std::string& name)
    {
        std::filesystem::create_directory(folder);
        Tank::Map::Manifest manifest;
        manifest.name = name;
        std::string json, error;
        Check(Tank::Map::SerializeManifest(manifest, json, error), "Serialize map");
        std::ofstream(folder / "Manifest.json", std::ios::binary) << json;
    }
}

int main()
{
    using namespace Tank::Map;
    try
    {
        TestRoot root;
        const std::string unicodeName = "Training \xe8\xa8\x93\xe7\xb7\xb4";
        WriteMap(root.path / "StorageA", unicodeName);
        WriteMap(root.path / "Legacy", "");
        std::filesystem::create_directory(root.path / "Templates");
        std::vector<NamedMap> maps;
        std::string error;
        Check(LoadMapCatalog(root.path, maps, error) && maps.size() == 2, "Catalog skips folders without a manifest");
        NamedMap found;
        Check(ResolveMapByName(maps, unicodeName, found, error) && found.folder.filename() == "StorageA",
            "Resolve UTF-8 map name independently of folder");
        Check(MapListLabel(found.document, found.folder) == "StorageA  -  " + unicodeName, "List shows folder and name");
        Check(ResolveMapByName(maps, "Legacy", found, error), "Legacy map uses folder name");
        Check(!ResolveMapByName(maps, "legacy", found, error), "Name lookup is case sensitive");
        Check(!ResolveMapByName(maps, "Missing", found, error) && found.folder.filename() == "Legacy",
            "Missing lookup preserves output");
        Check(!IsMapNameAvailable(maps, root.path / "Legacy", unicodeName, error), "Duplicate rename rejected");
        Check(IsMapNameAvailable(maps, root.path / "StorageA", unicodeName, error), "Own saved name is allowed");
        Check(!IsMapNameAvailable(maps, root.path / "StorageA", "Legacy", error), "Legacy fallback also reserves its name");

        std::filesystem::rename(root.path / "StorageA", root.path / "MovedStorage");
        Check(LoadMapCatalog(root.path, maps, error) && ResolveMapByName(maps, unicodeName, found, error) &&
            found.folder.filename() == "MovedStorage", "Explicit name survives folder rename");
        MapFolder session;
        Check(session.Open(found.folder, error), "Open named map");
        auto renamed = session.Document();
        renamed.name = "Next arena";
        renamed.playable = false;
        Check(session.SetManifest(renamed, error) && session.Save(error), "Rename and save");
        Check(LoadMapCatalog(root.path, maps, error) && ResolveMapByName(maps, "Next arena", found, error),
            "Saved rename updates reference key");
        Check(!found.document.playable, "Catalog retains playable state and permits editor lookup of nonplayable maps");
        Check(!ResolveMapByName(maps, unicodeName, found, error), "Old name no longer resolves after rename");
        WriteMap(root.path / "Duplicate", "Next arena");
        Check(LoadMapCatalog(root.path, maps, error) && !ResolveMapByName(maps, "Next arena", found, error) &&
            error.find("Ambiguous") != std::string::npos, "Duplicate names never silently choose a map");

        Check(!IsValidMapName("", error) && !IsValidMapName(" leading", error) &&
            !IsValidMapName("trailing ", error) && !IsValidMapName("line\nbreak", error) &&
            !IsValidMapName(std::string(257, 'a'), error), "Invalid reference names rejected");
        Check(IsValidMapName(unicodeName, error), "Unicode names accepted");
        Manifest manifest, unchanged;
        manifest.name = "Kept";
        std::string json;
        Check(SerializeManifest(manifest, json, error), "Serialize named manifest");
        const auto value = json.find("Kept");
        json.replace(value, 4, "");
        unchanged.name = "Preserved";
        Check(!DeserializeManifest(json, unchanged, error) && unchanged.name == "Preserved",
            "Explicit empty name rejected without changing output");
        manifest.name = "invalid\nname";
        json = "Preserved";
        Check(!SerializeManifest(manifest, json, error) && json == "Preserved", "Invalid name cannot serialize");
        std::cout << "Map catalog tests passed\n";
        return 0;
    }
    catch (const std::exception& exception)
    {
        std::cerr << exception.what() << '\n';
        return 1;
    }
}
