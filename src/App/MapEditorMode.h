#pragma once

#include "Map/MapFolder.h"
#include "App/UndoHistory.h"
#include "App/ExternalUndoEdit.h"
#include <tuple>
#include "App/SnapshotUndoEdit.h"
#include "App/ManifestUndoEdit.h"
#include "Physics/EnemyAttackType.h"
#include "Ui/EnemyUnitEditorWindow.h"
#include <array>
#include <functional>
#include <optional>
#include <string>
#include <unordered_set>
#include <utility>
#include "Map/GltfRoles.h"

struct HWND__;
namespace Engine { struct CameraState; }

class MapEditorMode
{
public:
    // Returns true when the user has completed a request to return to the menu.
    bool DrawUi(HWND__* owner, bool* outputWindowVisible = nullptr);
    template<class State, class Apply>
    void ObserveViewEdit(const State& before, const State& after, std::uint64_t interaction,
        Apply apply, const char* name)
    {
        if (!m_map.IsOpen()) return;
        if (!(before == after))
        {
            FlushPendingEnemyCatalogEdit();
            FlushPendingManifestEdit();
            FlushPendingGizmoEdit();
        }
        m_viewEdit.Update(before, after, interaction, m_undoHistory, apply, name);
    }
    bool Undo();
    bool Redo();
    bool CanUndo() const { return m_map.IsOpen() && (m_undoHistory.CanUndo() ||
        m_viewEdit.HasPending() || m_enemyCatalogEdit.HasPending() || m_manifestEdit.HasPending() || m_gizmoEdit.HasPending()); }
    bool CanRedo() const { return m_map.IsOpen() && m_undoHistory.CanRedo(); }
    void DrawPlacementGizmo(const Engine::CameraState& camera, float aspectRatio);
    bool PlacementGizmoCapturesMouse() const;
    void UpdateViewportPicking(const std::function<std::optional<std::string>(float, float)>& pick);
    void RequestExit();
    void RequestApplicationExit();
    using AssetValidator = std::function<bool(const std::filesystem::path&, const Tank::Map::GltfRoles&, std::string&)>;
    void SetAssetValidator(AssetValidator validator) { m_assetValidator = std::move(validator); }
    bool ConsumeSceneReloadRequest();
    bool ConsumeFocusSelectedRequest();
    void RequestFocusSelected() { m_sceneReloadRequested = true; m_focusSelectedRequested = true; }
    std::string SelectedTreeNodeId() const { return SelectedNodeId(); }
    bool ConsumeApplicationExitApproval();
    std::optional<std::filesystem::path> ConsumeClosedMapFolder();
    bool HasUnsavedChanges() const { return m_map.IsDirty(); }
    bool IsMapOpen() const { return m_map.IsOpen(); }
    const Tank::Map::MapFolder& Map() const { return m_map; }
    float GridSpacingMeters() const { return m_gridSpacingMeters; }
    int GridHalfCellCount() const { return m_gridHalfCellCount; }
    bool GridVisible() const { return m_showGrid; }
    float GridLineWidthMeters() const { return m_gridLineWidthMeters; }
    bool CameraWindowVisible() const { return m_showCameraWindow; }
    void SetCameraWindowVisible(bool visible)
    {
        const bool before = m_showCameraWindow;
        m_showCameraWindow = visible;
        ObserveViewEdit(before, visible, 0, [this](bool value) { m_showCameraWindow = value; }, "Camera window visibility");
    }
    bool ShowVisualMeshes() const { return m_showVisualMeshes; }
    bool ShowHitMeshes() const { return m_showHitMeshes; }
    const std::array<float, 3>& VisualMeshColor() const { return m_visualMeshColor; }
    const std::string& SelectedEnemyId() const { return m_selectedEnemyId; }
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
    bool SaveSelectedMaterialColor();
    bool AddIndestructibleBox();
    bool UpdateSelectedInstance(const Tank::Map::Transform& transform);
    void SetSelectedInstanceVisible(bool visible);
    bool DuplicateSelectedInstance();
    bool RemoveSelectedInstance();
    bool UpdatePlayerSpawn(const Tank::Map::Transform& transform);
    bool AddClearArea();
    bool UpdateSelectedClearArea(const Tank::Map::ClearArea& area);
    bool RemoveSelectedClearArea();
    void DrawEnemies();
    bool LoadEnemyCatalog();
    bool AddSelectedEnemy();
    bool UpdateSelectedEnemy(const Tank::Map::EnemyPlacement& enemy);
    bool RemoveSelectedEnemy();
    bool CommitManifestEdit(const Tank::Map::Manifest& before,
        const Tank::Map::Manifest& after, const char* name);
    void RecordManifestEdit(const Tank::Map::Manifest& before,
        const Tank::Map::Manifest& after, const char* name);
    void ApplyManifestSnapshot(const Tank::Map::Manifest& manifest);
    void FlushPendingManifestEdit();
    void FlushPendingGizmoEdit();
    void FlushPendingEnemyCatalogEdit();
    void ClearUndoHistory();
    void ApplyEnemyCatalogSnapshot(const Tank::Physics::EnemyEditorSettings& settings);
    void DrawCheatSheet();
    void DrawMapTree();
    std::string SelectedNodeId() const;

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
    std::vector<Tank::Map::GltfMaterialColor> m_materialColors;
    int m_selectedMaterialIndex = 0;
    std::array<float, 4> m_materialColorEdit{ 1.0f, 1.0f, 1.0f, 1.0f };
    bool m_materialColorLoaded = false;
    bool m_materialOverrideFile = false;
    std::string m_materialCopyName;
    std::string m_selectedInstanceId;
    std::unordered_set<std::string> m_hiddenInstanceIds;
    std::string m_selectedClearAreaId;
    std::string m_selectedEnemyId;
    std::string m_selectedNodeId = "root";
    std::string m_enemyCatalogPath;
    Tank::Physics::EnemyEditorSettings m_enemyCatalog;
    Ui::EnemyUnitEditorWindow m_enemyUnitEditor;
    std::string m_enemyCatalogError;
    std::vector<std::string> m_enemyTypes;
    std::string m_selectedEnemyType;
    bool m_enemyCatalogInitialized = false;
    bool m_gizmoEnabled = true;
    bool m_viewportPickPending = false;
    bool m_revealTreeSelection = false;
    std::string m_renameNodeId;
    std::string m_renameNodeName;
    std::string m_renameNodeError;
    enum class GizmoOperation { Translate, Rotate, Scale };
    GizmoOperation m_gizmoOperation = GizmoOperation::Translate;
    bool m_gizmoLocal = false;
    bool m_playerStartGizmoSelected = false;
    bool m_sceneReloadRequested = false;
    bool m_focusSelectedRequested = false;
    bool m_applicationExitApproved = false;
    std::optional<std::filesystem::path> m_closedMapFolder;
    AssetValidator m_assetValidator;
    bool m_showGrid = true;
    bool m_gridSnapEnabled = false;
    float m_gridSpacingMeters = 1.0f;
    int m_gridHalfCellCount = 10;
    float m_gridLineWidthMeters = 0.02f;
    bool m_showVisualMeshes = true;
    bool m_showHitMeshes = false;
    std::array<float, 3> m_visualMeshColor = { 80.0f / 255.0f, 80.0f / 255.0f, 80.0f / 255.0f };
    bool m_showCheatSheet = true;
    bool m_showCameraWindow = true;
    Tank::App::UndoHistory m_undoHistory;
    Tank::App::SnapshotUndoEdit<Tank::Physics::EnemyEditorSettings> m_enemyCatalogEdit;
    Tank::App::ManifestUndoEdit m_manifestEdit;
    Tank::App::ManifestUndoEdit m_gizmoEdit;
    Tank::App::ExternalUndoEdit m_viewEdit;
    bool m_replayingUndo = false;
};
