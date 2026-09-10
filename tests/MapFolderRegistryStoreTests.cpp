#include "App/MapFolderRegistryStore.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>

namespace
{
    bool Check(bool condition, const char* message)
    {
        if (!condition) std::cerr << "FAIL MapFolderRegistryStore: " << message << '\n';
        return condition;
    }
}

int main()
{
    bool passed = true;
    const std::filesystem::path testRoot =
        std::filesystem::current_path() / "MapFolderRegistryStoreTestsTemp";
    std::filesystem::remove_all(testRoot);
    Tank::App::MapFolderRegistryStore store(testRoot);

    const std::vector<std::filesystem::path> source = {
        std::filesystem::absolute(testRoot / "Map One").lexically_normal(),
        std::filesystem::absolute(testRoot / "MapTwo").lexically_normal()
    };
    std::string status;
    passed &= Check(store.Write(source, status), "write must succeed");

    std::vector<std::filesystem::path> loaded;
    passed &= Check(store.Read(loaded, status), "read must succeed after write");
    passed &= Check(loaded == source, "folder paths must round trip in order");

    {
        std::ofstream output(store.Path(), std::ios::binary | std::ios::trunc);
        output << "{invalid}";
    }
    loaded = { "preserved" };
    passed &= Check(!store.Read(loaded, status), "invalid JSON must fail");
    passed &= Check(loaded == std::vector<std::filesystem::path> { "preserved" },
        "failed read must preserve the caller's folders");

    std::filesystem::remove(store.Path());
    passed &= Check(!store.Read(loaded, status), "missing registry must fail cleanly");

    std::filesystem::remove_all(testRoot);
    if (!passed) return 1;
    std::cout << "PASS MapFolderRegistryStore\n";
    return 0;
}
