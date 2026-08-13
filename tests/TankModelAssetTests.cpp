#include <GltfLoader.h>
#include <Scene/SceneBuilder.h>

#include <algorithm>
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
    for (const char* expected : { "Body", "Cannon", "Side" })
    {
        if (std::find(names.begin(), names.end(), expected) == names.end())
        {
            std::cerr << "Missing node: " << expected << '\n';
            return 1;
        }
    }

    Engine::SceneBuilder builder;
    const Engine::GltfNodeMeshAddResult body =
        builder.AddGltfNodeMesh(loaded.asset, "Body");
    const Engine::GltfNodeMeshAddResult cannon =
        builder.AddGltfNodeMesh(loaded.asset, "Cannon");
    const Engine::GltfNodeMeshAddResult side =
        builder.AddGltfNodeMesh(loaded.asset, "Side");
    if (!body || !cannon || !side ||
        body.meshId == cannon.meshId || body.meshId == side.meshId ||
        cannon.meshId == side.meshId)
    {
        std::cerr << "Tank parts were not converted to distinct meshes\n";
        return 1;
    }

    return 0;
}
