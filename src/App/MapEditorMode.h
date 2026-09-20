#pragma once

#include "Map/MapFolder.h"
#include "Map/GltfRoles.h"

#include <array>
#include <functional>
#include <optional>
#include <string>
#include <unordered_set>
#include <utility>

struct HWND__;

class MapEditorMode
{
public:
    // Returns true when the user has completed a request to return to the menu.
    bool DrawUi(HWND__* owner);
    void RequestExit();
    void RequestApplicationExit();
    using AssetValidator = std::function<bool(const std::filesystem::path&, const Tank::Map::GltfRoles&, std::string&)>;
    void SetAssetValidator(AssetValidator validator) { m_assetValidator = std::move(validator); }
    bool ConsumeSceneReloadRequest();
    bool ConsumeFocusSelectedRequest();
    bool ConsumeApplicationExitApproval();
    std::optional<std::filesystem::path> ConsumeClosedMapFolder();
    bool HasUnsavedChanges() const { return m_map.IsDirty(); }
    bool IsMapOpen() const { return m_map.IsOpen(); }
    const Tank::Map::MapFolder& Map() const { return m_map; }
    float GridSpacingMeters() const { return m_gridSpacingMeters; }
    int GridHalfCellCount() const { return m_gridHalfCellCount; }
    float GridLineWidthMeters() const { return m_gridLineWidthMeters; }
    bool ShowVisualMeshes() const { return m_showVisualMeshes; }
    bool ShowHitMeshes() const { return m_showHitMeshes; }
    const std::array<float, 3>& VisualMeshColor() const { return m_visualMeshColor; }
    const std::string& SelectedInstanceId() const { return m_selectedInstanceId; }
    const std::unordered_set<std::string>& HiddenInstanceIds() const { return m_hiddenInstanceIds; }
    void SetPreviewError(const std::string& error) { m_status = "Preview failed: " + error; }
    void SetPreviewWarning(const std::string& warning) { m_previewWarning = warning; }

private:
    enum class Action { None, Open, OpenSelected, Create, Exit, ExitApplication };
    void Request(Action action);
    bool Execute(HWND__* owner);
    bool Save();
    void RefreshAssets();
    void RefreshAvailableMaps();
    bool OpenMapFolder(const std::filesystem::path& folder, std::string& error);
    std::filesystem::path MapAssetsRoot() const;
    bool AddSelectedModel();
    bool AddIndestructibleBox();
    bool UpdateSelectedInstance(const Tank::Map::Transform& transform);
    void SetSelectedInstanceVisible(bool visible);
    bool DuplicateSelectedInstance();
    bool RemoveSelectedInstance();
    bool UpdatePlayerSpawn(const Tank::Map::Transform& transform);
    bool AddClearArea();
    bool UpdateSelectedClearArea(const Tank::Map::ClearArea& area);
    bool RemoveSelectedClearArea();
    void DrawCheatSheet();

    Tank::Map::MapFolder m_map;
    Action m_pending = Action::None;
    std::filesystem::path m_pendingFolder;
    bool m_confirm = false;
    std::string m_status;
    std::string m_previewWarning;
    std::vector<std::string> m_assets;
    std::vector<std::filesystem::path> m_availableMaps;
    std::filesystem::path m_selectedAvailableMap;
    bool m_availableMapsScanned = false;
    std::string m_selectedAsset;
    std::string m_assetError;
    Tank::Map::GltfRoles m_roles;
    std::string m_inspectedAsset;
    std::string m_roleError;
    std::string m_selectedInstanceId;
    std::unordered_set<std::string> m_hiddenInstanceIds;
    std::string m_selectedClearAreaId;
    bool m_sceneReloadRequested = false;
    bool m_focusSelectedRequested = false;
    bool m_applicationExitApproved = false;
    std::optional<std::filesystem::path> m_closedMapFolder;
    AssetValidator m_assetValidator;
    float m_gridSpacingMeters = 1.0f;
    int m_gridHalfCellCount = 10;
    float m_gridLineWidthMeters = 0.02f;
    bool m_showVisualMeshes = true;
    bool m_showHitMeshes = false;
    std::array<float, 3> m_visualMeshColor = { 80.0f / 255.0f, 80.0f / 255.0f, 80.0f / 255.0f };
    bool m_showCheatSheet = true;
};
