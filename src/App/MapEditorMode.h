#pragma once

#include "Map/MapFolder.h"
#include "Map/GltfRoles.h"

#include <functional>
#include <optional>
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
    bool ConsumeApplicationExitApproval();
    std::optional<std::filesystem::path> ConsumeClosedMapFolder();
    bool HasUnsavedChanges() const { return m_map.IsDirty(); }
    bool IsMapOpen() const { return m_map.IsOpen(); }
    const Tank::Map::MapFolder& Map() const { return m_map; }
    float GridSpacingMeters() const { return m_gridSpacingMeters; }
    int GridHalfCellCount() const { return m_gridHalfCellCount; }
    float GridLineWidthMeters() const { return m_gridLineWidthMeters; }
    void SetPreviewError(const std::string& error) { m_status = "Preview failed: " + error; }

private:
    enum class Action { None, Open, Exit, ExitApplication };
    void Request(Action action);
    bool Execute(HWND__* owner);
    bool Save();
    void RefreshAssets();
    bool AddSelectedModel();
    bool UpdateSelectedInstance(const Tank::Map::Transform& transform);
    bool DuplicateSelectedInstance();
    bool RemoveSelectedInstance();
    bool UpdatePlayerSpawn(const Tank::Map::Transform& transform);
    bool AddClearArea();
    bool UpdateSelectedClearArea(const Tank::Map::ClearArea& area);
    bool RemoveSelectedClearArea();
    void DrawCheatSheet();

    Tank::Map::MapFolder m_map;
    Action m_pending = Action::None;
    bool m_confirm = false;
    std::string m_status;
    std::vector<std::string> m_assets;
    std::string m_selectedAsset;
    std::string m_assetError;
    Tank::Map::GltfRoles m_roles;
    std::string m_inspectedAsset;
    std::string m_roleError;
    std::string m_selectedInstanceId;
    std::string m_selectedClearAreaId;
    bool m_sceneReloadRequested = false;
    bool m_applicationExitApproved = false;
    std::optional<std::filesystem::path> m_closedMapFolder;
    AssetValidator m_assetValidator;
    float m_gridSpacingMeters = 1.0f;
    int m_gridHalfCellCount = 10;
    float m_gridLineWidthMeters = 0.02f;
    bool m_showCheatSheet = true;
};
