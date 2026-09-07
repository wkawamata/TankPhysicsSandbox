#include "Map/MapAssetCatalog.h"
#include "Map/MapManifest.h"

#define NOMINMAX
#include <Windows.h>

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

    struct TestFolder
    {
        std::filesystem::path path = std::filesystem::temp_directory_path() /
            ("TankMapCatalogTests-" + std::to_string(GetCurrentProcessId()) + "-" +
                std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        TestFolder() { Check(std::filesystem::create_directory(path), "Create test folder"); }
        ~TestFolder() { std::error_code error; std::filesystem::remove_all(path, error); }
    };

    void Touch(const std::filesystem::path& path)
    {
        std::ofstream file(path);
        Check(static_cast<bool>(file), "Create fixture");
    }
}

int main()
{
    using namespace Tank::Map;
    try
    {
        TestFolder temporary;
        const auto root = temporary.path / L"Map \u65e5\u672c\u8a9e";
        std::filesystem::create_directory(root);
        std::vector<std::string> assets = { "old.glb" };
        std::string error = "previous error";
        Check(FindMapAssets(root, assets, error) && assets.empty() && error.empty(), "Empty map succeeds");
        std::filesystem::create_directories(root / "Models/Nested");
        std::filesystem::create_directory(root / "directory.glb");
        Touch(root / "z.glb");
        Touch(root / "A.GLTF");
        Touch(root / "Models/Nested/model.Glb");
        Touch(root / "Models/mesh##1.gltf");
        Touch(root / L"Models/\u5efa\u7269.glb");
        Touch(root / "ignored.bin");
        Touch(root / "ignored.gltf.backup");
        Touch(root / "Manifest.json");
        const auto unicode = std::filesystem::path(L"Models/\u5efa\u7269.glb").generic_u8string();
        const std::vector<std::string> expected = {
            "A.GLTF", "Models/Nested/model.Glb", "Models/mesh##1.gltf",
            std::string(unicode.begin(), unicode.end()), "z.glb" };
        Check(FindMapAssets(root, assets, error) && assets == expected, "Recursive filtered sorted UTF-8 relative paths");
        Manifest manifest;
        for (size_t index = 0; index < assets.size(); ++index)
            manifest.instances.push_back({ "model-" + std::to_string(index), assets[index], {} });
        std::string json;
        Check(SerializeManifest(manifest, json, error), "Catalog paths are accepted by Manifest");
        Check(!FindMapAssets(root / "missing", assets, error) && assets == expected && !error.empty(),
            "Failed scan preserves previous results");
        Check(!FindMapAssets(root / "z.glb", assets, error), "Regular file cannot be map root");
        Check(!FindMapAssets({}, assets, error), "Empty root rejected");
        std::filesystem::remove(root / "A.GLTF");
        Touch(root / "new.glb");
        Check(FindMapAssets(root, assets, error) && assets.size() == expected.size() &&
            assets.front() == "Models/Nested/model.Glb" && assets[assets.size() - 2] == "new.glb",
            "Refresh reflects added and removed files");

        // Create a symbolic link when the machine grants that privilege, pointing to
        // a sibling folder to verify that recursive scans cannot escape the root.
        const auto external = temporary.path / "external";
        const auto link = root / "linked";
        std::filesystem::create_directory(external);
        Touch(external / "outside.glb");
        std::error_code linkError;
        std::filesystem::create_directory_symlink(external, link, linkError);
        if (!linkError)
        {
            const auto before = assets;
            Check(FindMapAssets(root, assets, error) && assets == before, "Directory links are excluded");
            std::filesystem::remove(link);
        }
        else std::cout << "SKIP symbolic-link check: " << linkError.message() << '\n';
        std::cout << "Map asset catalog tests passed\n";
        return 0;
    }
    catch (const std::exception& exception)
    {
        std::cerr << exception.what() << '\n';
        return 1;
    }
}
