#pragma once

#include "Map/MapManifest.h"
#include "Map/GltfRoles.h"
#include "Scene/SceneBuilder.h"

#include <filesystem>
#include <memory>

namespace Tank::Rendering
{
    struct MapEditorGridSettings
    {
        float spacingMeters = 1.0f;
        int halfCellCount = 10;
        float lineWidthMeters = 0.02f;
    };

    // Rendering-only adapter for the map editor. TankMapCore remains renderer-independent.
    class MapEditorScenePresenter
    {
    public:
        bool Rebuild(const std::filesystem::path& mapFolder, const Map::Manifest& manifest,
            const MapEditorGridSettings& grid, std::string& error);
        bool ValidateVisualAsset(const std::filesystem::path& assetPath,
            const Map::GltfRoles& roles, std::string& error) const;
        Engine::Scene& GetScene() { return m_builder->GetScene(); }
        void Clear();

    private:
        std::unique_ptr<Engine::SceneBuilder> m_builder = std::make_unique<Engine::SceneBuilder>();
    };
}
