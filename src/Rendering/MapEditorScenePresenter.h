#pragma once

#include "Map/MapManifest.h"
#include "Map/GltfRoles.h"
#include "Scene/SceneBuilder.h"

#include <array>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <unordered_set>

namespace Tank::Rendering
{
    struct MapEditorGridSettings
    {
        float spacingMeters = 1.0f;
        int halfCellCount = 10;
        float lineWidthMeters = 0.02f;
        bool visible = true;
    };

    struct MapEditorPreviewSettings
    {
        std::string selectedInstanceId;
        std::unordered_set<std::string> hiddenInstanceIds;
        bool showVisualMeshes = true;
        bool showHitMeshes = false;
        std::array<float, 3> visualMeshColor = {
            80.0f / 255.0f, 80.0f / 255.0f, 80.0f / 255.0f };
        std::string selectedEnemyId;
        std::string selectedNodeId;
    };

    struct MapEditorFocusTarget
    {
        std::array<float, 3> center = {};
        float radius = 0.5f;
    };

    // Rendering-only adapter for the map editor. TankMapCore remains renderer-independent.
    class MapEditorScenePresenter
    {
    public:
        bool Rebuild(const std::filesystem::path& mapFolder, const Map::Manifest& manifest,
            const MapEditorGridSettings& grid, std::string& error);
        bool Rebuild(const std::filesystem::path& mapFolder, const Map::Manifest& manifest,
            const MapEditorGridSettings& grid, const MapEditorPreviewSettings& preview,
            std::string& error);
        bool ValidateVisualAsset(const std::filesystem::path& assetPath,
            const Map::GltfRoles& roles, std::string& error) const;
        Engine::Scene& GetScene() { return m_builder->GetScene(); }
        const std::optional<MapEditorFocusTarget>& SelectedFocusTarget() const
        {
            return m_selectedFocusTarget;
        }
        const std::string& Warning() const { return m_warning; }
        // CPU triangle picking against visible authored geometry; grid and selection decorations are excluded.
        std::optional<std::string> PickRay(const std::array<float, 3>& origin,
            const std::array<float, 3>& direction) const;
        std::optional<std::string> PickScreen(float x, float y, float width, float height) const;
        void Clear();

    private:
        std::unique_ptr<Engine::SceneBuilder> m_builder = std::make_unique<Engine::SceneBuilder>();
        std::optional<MapEditorFocusTarget> m_selectedFocusTarget;
        struct PickRange
        {
            std::string nodeId;
            size_t instanceIndex;
            uint32_t firstIndex;
            uint32_t indexCount;
        };
        std::vector<PickRange> m_pickRanges;
        std::string m_warning;
    };
}
