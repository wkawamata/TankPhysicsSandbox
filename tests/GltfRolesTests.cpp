#include "Map/GltfRoles.h"
#include <nlohmann/json.hpp>
#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace
{
    using Json = nlohmann::json;
    void Check(bool condition, const char* message)
    {
        if (!condition) throw std::runtime_error(message);
    }
    Json Fixture()
    {
        return Json::parse(R"({"asset":{"version":"2.0"},"scene":0,"scenes":[{"nodes":[0]}],
            "nodes":[{"children":[1,3]},{"name":"Visual","children":[2]},
            {"name":"Wall","mesh":0},{"name":"Hit","children":[4]},{"mesh":0}],"meshes":[{}]})");
    }
    void Word(std::ofstream& file, uint32_t value)
    {
        for (int byte = 0; byte < 4; ++byte) file.put(static_cast<char>((value >> (8 * byte)) & 255));
    }
    void Glb(const std::filesystem::path& path, std::string text, uint32_t version = 2, bool badTail = false)
    {
        while (text.size() % 4) text += ' ';
        std::ofstream file(path, std::ios::binary);
        Word(file, 0x46546c67); Word(file, version); Word(file, static_cast<uint32_t>(text.size()) + 20 + (badTail ? 1 : 0));
        Word(file, static_cast<uint32_t>(text.size())); Word(file, 0x4e4f534a);
        file << text;
        if (badTail) file.put('x');
    }
    struct Temporary
    {
        std::filesystem::path path = std::filesystem::temp_directory_path() /
            ("TankGltfRoles-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        Temporary() { Check(std::filesystem::create_directory(path), "Create fixture folder"); }
        ~Temporary() { std::error_code error; std::filesystem::remove_all(path, error); }
    };
}

int main()
{
    using namespace Tank::Map;
    try
    {
        GltfRoles roles;
        std::string error;
        const auto valid = Fixture();
        Check(ParseGltfRoles(valid.dump(), roles, error), "Read Visual/Hit hierarchy");
        Check(roles.meshNodes.size() == 2 && roles.meshNodes[0].role == MeshRole::Visual &&
            roles.meshNodes[1].role == MeshRole::Hit && roles.meshNodes[1].nodeIndex == 4 &&
            roles.meshNodes[1].name.empty(), "Roles use node identity, allow unnamed mesh and shared mesh");
        const auto reject = [&](const Json& json)
        {
            Check(!ParseGltfRoles(json.dump(), roles, error) && !error.empty() && roles.meshNodes.size() == 2,
                "Invalid hierarchy rejected without changing output");
        };
        auto changed = valid; changed["nodes"][4]["children"] = {0}; reject(changed);
        changed = valid; changed["nodes"][3]["children"] = {2}; reject(changed);
        changed = valid; changed["nodes"][1]["children"] = {3}; reject(changed);
        changed = valid; changed["nodes"][3]["name"] = "hit"; reject(changed);
        changed = valid; changed["nodes"][3]["mesh"] = 0; reject(changed);
        changed = valid; changed["nodes"][4].erase("mesh"); reject(changed);
        changed = valid; changed["nodes"][2]["mesh"] = 1; reject(changed);
        changed = valid; changed["nodes"][1]["children"] = {-1}; reject(changed);
        changed = valid; changed["scene"] = 0.5; reject(changed);
        changed = valid; changed["asset"]["version"] = "1.0"; reject(changed);
        changed = valid; changed["nodes"][4]["name"] = 10; reject(changed);
        changed = valid; changed["scenes"].push_back({{"nodes", {0}}}); changed.erase("scene"); reject(changed);
        changed = valid; changed.erase("scene");
        Check(ParseGltfRoles(changed.dump(), roles, error), "Single scene default accepted");
        changed = valid; changed["nodes"].push_back({{"mesh", 999}});
        Check(ParseGltfRoles(changed.dump(), roles, error), "Inactive nodes do not enter active scene");
        changed = valid;
        for (int index = 5; index < 265; ++index)
        {
            changed["nodes"].push_back({{"children", {index + 1}}});
        }
        changed["nodes"].push_back(Json::object()); changed["nodes"][2]["children"] = {5}; reject(changed);

        Temporary temporary;
        const auto path = temporary.path / L"\u5f79\u5272.GLB";
        Glb(path, valid.dump());
        Check(InspectGltfRoles(path, roles, error) && error.empty(), "GLB and Unicode path");
        Glb(path, valid.dump(), 1); Check(!InspectGltfRoles(path, roles, error), "Reject GLB version 1");
        Glb(path, valid.dump(), 2, true); Check(!InspectGltfRoles(path, roles, error), "Reject truncated trailing chunk");
        { std::ofstream file(path, std::ios::binary); file << "glTF"; }
        Check(!InspectGltfRoles(path, roles, error), "Reject truncated GLB header");
        const auto gltf = temporary.path / "test.gltf";
        { std::ofstream file(gltf); file << valid; }
        Check(InspectGltfRoles(gltf, roles, error), "glTF JSON file");

        const std::filesystem::path meshFixture =
            std::filesystem::path(__FILE__).parent_path() / "Fixtures" / "MapEditor" / "triangle-visual-hit.gltf";
        Check(InspectGltfRoles(meshFixture, roles, error) && roles.meshNodes.size() == 2,
            "Visual/Hit mesh fixture");
        { std::ofstream file(gltf); file << "{broken"; }
        Check(!InspectGltfRoles(gltf, roles, error), "Reject malformed JSON");
        Check(!InspectGltfRoles(temporary.path / "missing.gltf", roles, error), "Report missing file");
        std::cout << "glTF role tests passed\n";
        return 0;
    }
    catch (const std::exception& exception) { std::cerr << exception.what() << '\n'; return 1; }
}
