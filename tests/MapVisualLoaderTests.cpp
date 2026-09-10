#include "Map/GltfHitMesh.h"
#include "Map/MapManifest.h"
#include "Rendering/MapEditorScenePresenter.h"
#include "Rendering/MapVisualLoader.h"
#include "Scene/SceneBuilder.h"

#include <DirectXMath.h>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

namespace
{
    bool NearlyEqual(float a, float b)
    {
        return std::abs(a - b) < 0.0001f;
    }

    bool TestHitMeshOverlayGeometry()
    {
        Engine::SceneBuilder builder;
        const uint32_t material = builder.AddSolidColorMaterial(255, 45, 190, 255);
        Tank::Map::HitTriangleMesh hitMesh;
        hitMesh.vertices = { { 1.0f, 2.0f, 3.0f }, { 3.0f, 2.0f, 3.0f },
            { 1.0f, 2.0f, 5.0f } };
        hitMesh.triangles = { { 0u, 1u, 2u } };
        size_t instanceIndex = 99;
        std::string error;
        if (!Tank::Rendering::AppendMapHitMeshOverlay(
            builder, { hitMesh }, material, instanceIndex, error))
        {
            std::cerr << error << '\n';
            return false;
        }

        const Engine::SceneMesh& mesh = builder.GetMesh();
        const Engine::Scene& scene = builder.GetScene();
        if (mesh.ranges.size() != 1 || mesh.vertices.size() != 3 ||
            mesh.indices != std::vector<uint32_t>({ 0u, 1u, 2u }) ||
            scene.instances.size() != 1 || instanceIndex != 0 ||
            scene.instances[0].meshId != 0 || scene.instances[0].materialId != material)
        {
            return false;
        }
        const Engine::SceneVertex& vertex = mesh.vertices[0];
        return NearlyEqual(vertex.position.x, 1.0f) &&
            NearlyEqual(vertex.position.y, 2.0f) &&
            NearlyEqual(vertex.position.z, 3.0f) &&
            NearlyEqual(vertex.normal.x, 0.0f) &&
            NearlyEqual(vertex.normal.y, -1.0f) &&
            NearlyEqual(vertex.normal.z, 0.0f) &&
            NearlyEqual(scene.instances[0].world._11, 0.0f);
    }

    bool TestFailureDoesNotMutateScene()
    {
        Engine::SceneBuilder builder;
        const uint32_t material = builder.AddSolidColorMaterial(255, 45, 190, 255);
        Tank::Map::HitTriangleMesh hitMesh;
        hitMesh.vertices = { { 0.0f, 0.0f, 0.0f } };
        hitMesh.triangles = { { 0u, 1u, 2u } };
        size_t instanceIndex = 77;
        std::string error;
        return !Tank::Rendering::AppendMapHitMeshOverlay(
                   builder, { hitMesh }, material, instanceIndex, error) &&
            !error.empty() && builder.GetMesh().vertices.empty() &&
            builder.GetMesh().indices.empty() && builder.GetMesh().ranges.empty() &&
            builder.GetScene().instances.empty() && instanceIndex == 77;
    }

    bool TestEditorMarkers()
    {
        Tank::Map::Manifest manifest;
        manifest.playerSpawn.position = { 2.0f, 3.0f, 4.0f };
        manifest.instances = { { "instance-1", "triangle-visual-hit.gltf", {} } };
        manifest.clearAreas = { { "clear-area-1", "Finish",
            { 5.0f, 1.0f, -2.0f }, { 4.0f, 2.0f, 6.0f } } };
        Tank::Rendering::MapEditorScenePresenter presenter;
        const Tank::Rendering::MapEditorGridSettings grid = { 1.0f, 1, 0.02f };
        std::string error;
        const std::filesystem::path fixtureFolder =
            std::filesystem::path(TANK_SOURCE_DIR) / "tests/Fixtures/MapEditor";
        if (!presenter.Rebuild(fixtureFolder, manifest, grid, error))
        {
            std::cerr << error << '\n';
            return false;
        }

        // 6 grid lines, 1 Visual, 3 player-start marker parts, and 12 AABB edges.
        const Engine::Scene& scene = presenter.GetScene();
        if (scene.instances.size() != 22)
            return false;
        const Engine::InstanceData& spawnPost = scene.instances[7];
        return NearlyEqual(spawnPost.world._14, 2.0f) &&
            NearlyEqual(spawnPost.world._24, 3.75f) &&
            NearlyEqual(spawnPost.world._34, 4.0f);
    }

    bool TestClearBeacons()
    {
        Engine::SceneBuilder builder;
        const uint32_t material = builder.AddSolidColorMaterial(70, 255, 90, 255);
        const Engine::SceneMeshId cube = builder.AddCube(1.0f);
        Tank::Map::Manifest manifest;
        manifest.clearAreas = {
            { "clear-area-1", "Finish", { 5.0f, 2.0f, -3.0f }, { 4.0f, 2.0f, 6.0f } },
            { "clear-area-2", "Bonus", { -4.0f, 1.0f, 8.0f }, { 2.0f, 3.0f, 2.0f } }
        };
        std::vector<size_t> indices;
        Tank::Rendering::AppendMapClearBeacons(builder, cube, manifest, material, indices);
        const Engine::Scene& scene = builder.GetScene();
        if (indices != std::vector<size_t>({ 0u, 1u }) || scene.instances.size() != 2u)
            return false;
        const Engine::InstanceData& first = scene.instances[0];
        return first.materialId == material &&
            NearlyEqual(first.world._11, 0.35f) &&
            NearlyEqual(first.world._22, 12.0f) &&
            NearlyEqual(first.world._33, 0.35f) &&
            NearlyEqual(first.world._14, 5.0f) &&
            NearlyEqual(first.world._24, 8.0f) &&
            NearlyEqual(first.world._34, -3.0f);
    }
}

int main()
{
    if (!TestHitMeshOverlayGeometry() || !TestFailureDoesNotMutateScene() ||
        !TestEditorMarkers() || !TestClearBeacons())
    {
        std::cerr << "Map visual loader tests failed.\n";
        return 1;
    }
    std::cout << "Map visual loader tests passed.\n";
    return 0;
}
