#include "Map/GltfHitMesh.h"
#include "Map/MapManifest.h"
#include "Rendering/MapEditorScenePresenter.h"
#include "Rendering/MapVisualLoader.h"
#include "Rendering/EnemyPlacementGizmo.h"
#include "Scene/SceneBuilder.h"

#include <DirectXMath.h>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

namespace
{
    bool TestIndestructibleBoxTemplate()
    {
        const auto folder = std::filesystem::path(TANK_SOURCE_DIR) / "Assets/Map/Templates";
        Tank::Map::HitTriangleMesh hit;
        std::string error;
        if (!Tank::Map::LoadGltfHitMesh(folder / "IndestructibleBox.gltf", hit, error))
        {
            std::cerr << error << '\n';
            return false;
        }
        if (hit.vertices.size() != 8 || hit.triangles.size() != 12) return false;
        // Closed, outward-facing collider on all six sides after LH conversion.
        for (const auto& triangle : hit.triangles)
        {
            const auto& a = hit.vertices[triangle[0]];
            const auto& b = hit.vertices[triangle[1]];
            const auto& c = hit.vertices[triangle[2]];
            const float ux = b[0] - a[0], uy = b[1] - a[1], uz = b[2] - a[2];
            const float vx = c[0] - a[0], vy = c[1] - a[1], vz = c[2] - a[2];
            const float outward = (uy * vz - uz * vy) * a[0] +
                (uz * vx - ux * vz) * (a[1] - 1.5f) + (ux * vy - uy * vx) * a[2];
            if (outward <= 0.0f) return false;
        }
        Tank::Map::Manifest manifest;
        manifest.instances.push_back({ "instance-1", "IndestructibleBox.gltf", {} });
        std::string json;
        Tank::Map::Manifest restored;
        if (!Tank::Map::SerializeManifest(manifest, json, error) ||
            !Tank::Map::DeserializeManifest(json, restored, error)) return false;
        Engine::SceneBuilder builder;
        const auto material = builder.AddSolidColorMaterial(110, 120, 130, 255);
        return Tank::Rendering::AppendMapVisuals(builder, folder, restored, material, error) &&
            builder.GetScene().instances.size() == 1 && builder.GetMesh().indices.size() == 36;
    }

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
            NearlyEqual(vertex.position.y,
                2.0f - Tank::Rendering::kHitMeshOverlayOffsetMeters) &&
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
        const Engine::InstanceData& firstGridLine = scene.instances[0];
        const Engine::InstanceData& spawnPost = scene.instances[7];
        return firstGridLine.world._24 + firstGridLine.world._22 * 0.5f > 0.0f &&
            NearlyEqual(spawnPost.world._14, 2.0f) &&
            NearlyEqual(spawnPost.world._24, 3.75f) &&
            NearlyEqual(spawnPost.world._34, 4.0f);
    }

    bool TestEditorSelectionHighlightAndVisibility()
    {
        Tank::Map::Manifest manifest;
        manifest.instances = {
            { "selected", "triangle-visual-hit.gltf", { { 4.0f, 2.0f, 6.0f }, {} } },
            { "hidden", "triangle-visual-hit.gltf", {} }
        };
        Tank::Rendering::MapEditorScenePresenter presenter;
        const Tank::Rendering::MapEditorGridSettings grid = { 1.0f, 1, 0.02f };
        const Tank::Rendering::MapEditorPreviewSettings preview = {
            "selected", { "hidden" }
        };
        std::string error;
        const std::filesystem::path fixtureFolder =
            std::filesystem::path(TANK_SOURCE_DIR) / "tests/Fixtures/MapEditor";
        if (!presenter.Rebuild(fixtureFolder, manifest, grid, preview, error))
        {
            std::cerr << error << '\n';
            return false;
        }

        // 6 grid lines, 1 visible model, 12 selection-box edges, and 3 player-start parts.
        const auto& focus = presenter.SelectedFocusTarget();
        const bool valid = presenter.GetScene().instances.size() == 22u && focus &&
            NearlyEqual(focus->center[0], 5.0f) && NearlyEqual(focus->center[1], 2.0f) &&
            NearlyEqual(focus->center[2], 5.0f) && NearlyEqual(focus->radius, std::sqrt(2.0f));
        if (!valid)
        {
            std::cerr << "Selection focus instances=" << presenter.GetScene().instances.size();
            if (focus)
                std::cerr << " center=" << focus->center[0] << ',' << focus->center[1] << ','
                          << focus->center[2] << " radius=" << focus->radius;
            else
                std::cerr << " missing";
            std::cerr << '\n';
        }
        return valid;
    }

    bool TestHitOnlyVisualFallbackAndDisplaySwitches()
    {
        Tank::Map::Manifest manifest;
        manifest.instances = { { "hit-only", "hit-only.gltf", {} } };
        Tank::Rendering::MapEditorScenePresenter presenter;
        const Tank::Rendering::MapEditorGridSettings grid = { 1.0f, 1, 0.02f };
        const std::filesystem::path fixtureFolder =
            std::filesystem::path(TANK_SOURCE_DIR) / "tests/Fixtures/MapEditor";
        std::string error;
        Tank::Rendering::MapEditorPreviewSettings preview;
        if (!presenter.Rebuild(fixtureFolder, manifest, grid, preview, error) ||
            presenter.GetScene().instances.size() != 10u || presenter.Warning().empty())
            return false;

        preview.showVisualMeshes = false;
        if (!presenter.Rebuild(fixtureFolder, manifest, grid, preview, error) ||
            presenter.GetScene().instances.size() != 9u)
            return false;

        preview.showHitMeshes = true;
        if (!presenter.Rebuild(fixtureFolder, manifest, grid, preview, error) ||
            presenter.GetScene().instances.size() != 10u)
            return false;
        const size_t hitInstance = presenter.GetScene().instances.size() -
            Tank::Rendering::kPlayerStartMarkerPartCount - 1u;
        const DirectX::XMFLOAT4X4& hitWorld = presenter.GetScene().instances[hitInstance].world;
        return hitWorld._11 == 1.0f && hitWorld._22 == 1.0f &&
            hitWorld._33 == 1.0f && hitWorld._44 == 1.0f;
    }

    bool TestEnemyGizmoTransforms()
    {
        using namespace Tank;
        for (const auto angles : { std::array<float, 3>{ 20, 135, -35 },
            std::array<float, 3>{ 90, 40, 30 }, std::array<float, 3>{ -90, 40, 30 },
            std::array<float, 3>{ 120, -80, 60 }, std::array<float, 3>{ 0, 0, 0 } })
        {
            const Map::EnemyPlacement source = { "enemy-1", "Fixed turret", { 3, 2, -4 }, angles };
            auto matrix = Rendering::EnemyPlacementMatrix(source);
            const auto rotated = Rendering::EnemyPlacementFromGizmo(source, matrix, true);
            const auto roundTrip = Rendering::EnemyPlacementMatrix(rotated);
            const float* first = &matrix._11;
            const float* second = &roundTrip._11;
            for (size_t i = 0; i < 16; ++i)
                if (std::abs(first[i] - second[i]) > 1.0e-4f) return false;
            matrix._41 += 5;
            const auto moved = Rendering::EnemyPlacementFromGizmo(source, matrix, false);
            if (moved.position != std::array<float, 3>{ 8, 2, -4 } ||
                moved.rotationDegrees != source.rotationDegrees ||
                moved.id != source.id || moved.unitType != source.unitType) return false;
        }
        return true;
    }

    bool TestEnemyEditorMarkers()
    {
        Tank::Map::Manifest manifest;
        manifest.enemies = { { "enemy-1", "Fixed turret", { 4, 2, 6 }, { 0, 90, 0 } } };
        Tank::Rendering::MapEditorScenePresenter presenter;
        Tank::Rendering::MapEditorPreviewSettings preview;
        preview.selectedEnemyId = "enemy-1";
        std::string error;
        if (!presenter.Rebuild({}, manifest, { 1.0f, 1, 0.02f }, preview, error)) return false;
        const auto& focus = presenter.SelectedFocusTarget();
        const auto& scene = presenter.GetScene();
        if (!focus || focus->center != manifest.enemies[0].position || scene.instances.size() != 12u)
            return false;
        // Grid has six parts, then body and two forward-marker parts.
        const auto& body = scene.instances[6];
        const auto& forward = scene.instances[7];
        if (!NearlyEqual(body.world._14, 4) || !NearlyEqual(body.world._24, 2) ||
            !NearlyEqual(body.world._34, 6) || !NearlyEqual(forward.world._14, 5) ||
            !NearlyEqual(forward.world._34, 6)) return false;
        manifest.nodes.push_back({"group", "Group", "root", {{10, 3, -2}, {}}});
        manifest.enemies[0].parentId = "group";
        if (!presenter.Rebuild({}, manifest, {1.0f, 1, 0.02f}, preview, error)) return false;
        const auto& parentedBody = presenter.GetScene().instances[6];
        if (!NearlyEqual(parentedBody.world._14, 14) || !NearlyEqual(parentedBody.world._24, 5) ||
            !NearlyEqual(parentedBody.world._34, 4) || !presenter.SelectedFocusTarget() ||
            presenter.SelectedFocusTarget()->center != std::array<float, 3>{14, 5, 4}) return false;
        manifest.startPoints.push_back({"second", {{20, 2, 30}, {}}, "root"});
        if (!presenter.Rebuild({}, manifest, {1.0f, 1, 0.02f}, preview, error) ||
            presenter.GetScene().instances.size() != 15u) return false;
        manifest.nodeInfo["player-start"] = {true, false};
        if (!presenter.Rebuild({}, manifest, {1.0f, 1, 0.02f}, preview, error) ||
            presenter.GetScene().instances.size() != 12u) return false;
        manifest.nodeInfo["enemy/enemy-1"] = {false, false};
        if (!presenter.Rebuild({}, manifest, {1.0f, 1, 0.02f}, preview, error) ||
            presenter.GetScene().instances.size() != 9u || presenter.SelectedFocusTarget()) return false;
        manifest.startPoints.clear();
        manifest.nodeInfo.clear();
        manifest.enemies.clear();
        if (!presenter.Rebuild({}, manifest, { 1.0f, 1, 0.02f }, preview, error)) return false;
        return !presenter.SelectedFocusTarget() && presenter.GetScene().instances.size() == 9u;
    }

    bool TestTreeFocusTargets()
    {
        Tank::Rendering::MapEditorScenePresenter presenter;
        Tank::Map::Manifest manifest;
        manifest.playerSpawn.position = {5, 2, 3};
        manifest.clearAreas.push_back({"area", "Area", {12,1,2}, {10,2,4}});
        Tank::Rendering::MapEditorPreviewSettings preview;
        preview.selectedNodeId = "player-start";
        std::string error;
        if (!presenter.Rebuild({}, manifest, {1,1,0.02f}, preview, error) ||
            !presenter.SelectedFocusTarget() || presenter.SelectedFocusTarget()->center != std::array<float,3>{5,2,3}) return false;
        preview.selectedNodeId = "area/area";
        if (!presenter.Rebuild({}, manifest, {1,1,0.02f}, preview, error)) return false;
        return presenter.SelectedFocusTarget() && presenter.SelectedFocusTarget()->center == std::array<float,3>{12,1,2} &&
            presenter.SelectedFocusTarget()->radius > 5;
    }

    bool TestRotatedClearAreaBoundary()
    {
        Tank::Map::Manifest manifest;
        manifest.nodeInfo["player-start"] = {true, false};
        manifest.clearAreas.push_back({"box", "Box", {10,2,0}, {4,2,1}, "root", {0,90,0}});
        Tank::Rendering::MapEditorScenePresenter presenter;
        std::string error;
        if (!presenter.Rebuild({}, manifest, {1,1,0.02f,false}, error)) return false;
        const auto& instances = presenter.GetScene().instances;
        if (instances.size() != 12) return false;
        const auto& edge = instances[0].world;
        return NearlyEqual(edge._14, 9.5f) && NearlyEqual(edge._24, 1) && NearlyEqual(edge._34, 0) &&
            NearlyEqual(std::abs(edge._31), 4) && NearlyEqual(edge._11, 0);
    }

    bool TestGridVisibility()
    {
        Tank::Rendering::MapEditorScenePresenter presenter;
        Tank::Map::Manifest manifest;
        manifest.playerSpawn.position = {0, 0, 0};
        Tank::Rendering::MapEditorGridSettings grid{1, 1, 0.02f};
        std::string error;
        if (!presenter.Rebuild({}, manifest, grid, error)) return false;
        const auto visibleCount = presenter.GetScene().instances.size();
        grid.visible = false;
        if (!presenter.Rebuild({}, manifest, grid, error)) return false;
        return visibleCount == presenter.GetScene().instances.size() + 6 &&
            presenter.PickRay({0, 0.75f, -10}, {0, 0, 1}) == "player-start";
    }

    bool TestViewportPicking()
    {
        using namespace Tank;
        Rendering::MapEditorScenePresenter presenter;
        Map::Manifest manifest;
        manifest.playerSpawn.position = {20, 0, 0};
        manifest.startPoints.push_back({"second", {{40, 0, 0}, {}}, "root"});
        manifest.enemies.push_back({"target", "Fixed turret", {10, 2, 0}, {}});
        manifest.clearAreas.push_back({"finish", "Finish", {30, 1, 0}, {2, 2, 2}});
        manifest.nodes.push_back({"parent", "Parent", "root", {{3, 0, 6}, {}}});
        manifest.instances = {
            {"back", "triangle-visual-hit.gltf", {}, "parent"},
            {"front", "triangle-visual-hit.gltf", {{0, 2, 0}, {}}, "parent"}
        };
        const auto folder = std::filesystem::path(TANK_SOURCE_DIR) / "tests/Fixtures/MapEditor";
        const auto build = [&](const Rendering::MapEditorPreviewSettings& preview = {})
        {
            std::string error;
            const bool ok = presenter.Rebuild(folder, manifest, {1, 1, 0.02f}, preview, error);
            if (!ok) std::cerr << error << '\n';
            return ok;
        };
        const auto expect = [&](std::array<float, 3> origin, std::array<float, 3> direction, const std::string& id)
        {
            const auto result = presenter.PickRay(origin, direction);
            if (result.value_or("") == id) return true;
            std::cerr << "Pick expected " << id << ", got " << result.value_or("none") << '\n';
            return false;
        };
        if (!build() || !expect({3.25f, 10, 5.75f}, {0, -1, 0}, "mesh/front") ||
            !expect({4.8f, 10, 4.2f}, {0, -1, 0}, "") ||
            !expect({10, 2, -10}, {0, 0, 1}, "enemy/target") ||
            !expect({20, 0.75f, -10}, {0, 0, 1}, "player-start") ||
            !expect({40, 0.75f, -10}, {0, 0, 1}, "start/second") ||
            !expect({31, 1, -10}, {0, 0, 1}, "area/finish") ||
            !expect({0, 10, 0}, {0, -1, 0}, "") ||
            !expect({0, 0, 0}, {0, 0, 0}, "")) return false;
        auto& camera = presenter.GetScene().camera;
        camera.pos = {10, 2, -10}; camera.gazePoint = {10, 2, 0};
        if (presenter.PickScreen(400, 300, 800, 600) != "enemy/target") return false;
        camera.projection = Engine::CameraProjection::Orthographic;
        if (presenter.PickScreen(400, 300, 800, 600) != "enemy/target" ||
            presenter.PickScreen(-1, 300, 800, 600) || presenter.PickScreen(0, 0, 0, 0)) return false;
        Rendering::MapEditorPreviewSettings preview;
        preview.hiddenInstanceIds.insert("front");
        if (!build(preview) || !expect({3.25f, 10, 5.75f}, {0, -1, 0}, "mesh/back")) return false;
        manifest.nodeInfo["mesh/back"] = {true, false};
        if (!build(preview) || !expect({3.25f, 10, 5.75f}, {0, -1, 0}, "")) return false;
        manifest.instances = {{"hit", "hit-only.gltf", {}}};
        if (!build() || !expect({0.25f, 10, -0.25f}, {0, -1, 0}, "mesh/hit")) return false;
        preview = {}; preview.showVisualMeshes = false;
        if (!build(preview) || !expect({0.25f, 10, -0.25f}, {0, -1, 0}, "")) return false;
        preview.showHitMeshes = true;
        if (!build(preview) || !expect({0.25f, 10, -0.25f}, {0, -1, 0}, "mesh/hit")) return false;
        presenter.Clear();
        return !presenter.PickRay({0, 10, 0}, {0, -1, 0});
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
    if (!TestIndestructibleBoxTemplate() || !TestHitMeshOverlayGeometry() || !TestFailureDoesNotMutateScene() ||
        !TestRotatedClearAreaBoundary() || !TestTreeFocusTargets() || !TestGridVisibility() || !TestViewportPicking() || !TestEditorMarkers() || !TestEditorSelectionHighlightAndVisibility() ||
        !TestHitOnlyVisualFallbackAndDisplaySwitches() || !TestEnemyEditorMarkers() || !TestEnemyGizmoTransforms() || !TestClearBeacons())
    {
        std::cerr << "Map visual loader tests failed.\n";
        return 1;
    }
    std::cout << "Map visual loader tests passed.\n";
    return 0;
}
