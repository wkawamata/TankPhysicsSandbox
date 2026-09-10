#include <GltfLoader.h>
#include <Scene/SceneBuilder.h>

#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

int main(int argc, char* argv[])
{
    if (argc != 2)
    {
        std::cerr << "Expected managed tank model path\n";
        return 1;
    }

    Engine::GltfSceneAssetLoadResult loaded =
        Engine::LoadGltfSceneAsset(std::filesystem::path(argv[1]).string());
    if (!loaded)
    {
        std::cerr << "Load failed: " << loaded.message << '\n';
        return 1;
    }

    std::vector<std::string> names = Engine::GetGltfMeshNodeNames(loaded.asset);
    if (names.size() != 1)
    {
        std::cerr << "Expected one merged tank mesh node\n";
        return 1;
    }

    Engine::SceneBuilder builder;
    const Engine::GltfNodeMeshAddResult body =
        builder.AddGltfNodeMesh(loaded.asset, names.front());
    if (!body)
    {
        std::cerr << "Merged tank node was not converted as the body mesh\n";
        return 1;
    }

    return 0;
}
