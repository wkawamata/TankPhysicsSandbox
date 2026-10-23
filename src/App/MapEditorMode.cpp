#include "MapEditorMode.h"

#include "Map/MapAssetCatalog.h"
#include "Map/MapEditing.h"
#include "Map/GatePrefab.h"
#include "Map/MapHierarchy.h"
#include "Ui/SceneTreeUI.h"
#include "Ui/MapListUi.h"
#include "Platform/Windows/MapFolderPicker.h"
#include "Physics/EnemyEditorJson.h"
#include "Rendering/EnemyPlacementGizmo.h"
#include <Scene/CameraView.h>
#include <Scene/CameraProjection.h>
#include <imgui.h>
#include <imgui_internal.h>
#include <ImGuiWidgets.h>
#include <third_party/ImGuizmo/ImGuizmo.h>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <algorithm>
#include <system_error>
#include <imgui.h>
#include <imgui_stdlib.h>

std::filesystem::path MapEditorMode::MapAssetsRoot() const
{
    const std::filesystem::path sourceRoot = TANK_SOURCE_ASSETS_MAP_DIR;
    std::error_code error;
    if (std::filesystem::is_directory(sourceRoot, error)) return sourceRoot;
    return std::filesystem::current_path() / "Assets" / "Map";
}

void MapEditorMode::RefreshAvailableMaps()
{
    m_availableMapsScanned = true;
    m_availableMaps.clear();
    std::string error;
    if (!Tank::Map::LoadMapCatalog(MapAssetsRoot(), m_availableMaps, error))
        m_status = "Map list: " + error;
    if (std::none_of(m_availableMaps.begin(), m_availableMaps.end(),
        [this](const auto& map) { return map.folder == m_selectedAvailableMap; }))
        m_selectedAvailableMap.clear();
}

bool MapEditorMode::OpenMapFolder(const std::filesystem::path& folder, std::string& error)
{
    if (!m_map.Open(folder, error)) return false;
    ClearUndoHistory();
    m_status = "Opened Manifest.json";
    m_mapNameDraft = m_map.Document().name;
    m_mapNameError.clear();
    m_previewWarning.clear();
    m_selectedAsset.clear();
    m_selectedNodeId = "root";
    m_selectedInstanceId.clear();
    m_focusSelectedRequested = false;
    m_hiddenInstanceIds.clear();
    m_selectedClearAreaId.clear();
    m_selectedEnemyId.clear();
    m_playerStartGizmoSelected = false;
    auto catalogPath = m_map.Folder() / "enemy_types.json";
    if (!std::filesystem::exists(catalogPath))
        catalogPath = std::filesystem::path(TANK_SOURCE_CONFIG_DIR) / "enemy_types.json";
    const auto catalogUtf8 = catalogPath.u8string();
    m_enemyCatalogPath.assign(catalogUtf8.begin(), catalogUtf8.end());
    m_enemyCatalogInitialized = true;
    m_enemyTypes.clear();
    m_selectedEnemyType.clear();
    LoadEnemyCatalog();
    RefreshAssets();
    RefreshAvailableMaps();
    m_selectedAvailableMap = m_map.Folder();
    m_sceneReloadRequested = true;
    return true;
}

void MapEditorMode::ApplyManifestSnapshot(const Tank::Map::Manifest& manifest)
{
    std::string error;
    if (!m_map.SetManifest(manifest, error))
    {
        m_status = "Could not apply undo state: " + error;
        return;
    }

    m_mapNameDraft = Tank::Map::MapName(manifest, m_map.Folder());
    m_mapNameError.clear();
    const auto tree = Tank::Map::MapTree(manifest);
    const auto hasNode = [&tree](const std::string& id)
    {
        return std::any_of(tree.begin(), tree.end(), [&](const auto& node) { return node.id == id; });
    };
    if (!hasNode(m_selectedNodeId)) m_selectedNodeId = "root";
    if (std::none_of(manifest.instances.begin(), manifest.instances.end(),
        [this](const auto& value) { return value.id == m_selectedInstanceId; }))
        m_selectedInstanceId.clear();
    if (std::none_of(manifest.enemies.begin(), manifest.enemies.end(),
        [this](const auto& value) { return value.id == m_selectedEnemyId; }))
        m_selectedEnemyId.clear();
    if (std::none_of(manifest.clearAreas.begin(), manifest.clearAreas.end(),
        [this](const auto& value) { return value.id == m_selectedClearAreaId; }))
        m_selectedClearAreaId.clear();
    m_sceneReloadRequested = true;
}

void MapEditorMode::RecordManifestEdit(const Tank::Map::Manifest& before,
    const Tank::Map::Manifest& after, const char* name)
{
    if (m_replayingUndo) return;
    m_viewEdit.Flush();
    std::string beforeJson;
    std::string afterJson;
    std::string error;
    if (!Tank::Map::SerializeManifest(before, beforeJson, error) ||
        !Tank::Map::SerializeManifest(after, afterJson, error) || beforeJson == afterJson)
        return;

    const auto beforeState = before;
    const auto afterState = after;
    m_undoHistory.RecordApplied(std::make_unique<Tank::App::LambdaUndoCommand>(
        name,
        [this, beforeState]
        {
            m_replayingUndo = true;
            ApplyManifestSnapshot(beforeState);
            m_replayingUndo = false;
        },
        [this, afterState]
        {
            m_replayingUndo = true;
            ApplyManifestSnapshot(afterState);
            m_replayingUndo = false;
        }));
}

bool MapEditorMode::CommitManifestEdit(const Tank::Map::Manifest& before,
    const Tank::Map::Manifest& after, const char* name)
{
    FlushPendingEnemyCatalogEdit();
    FlushPendingGizmoEdit();
    FlushPendingManifestEdit();
    std::string error;
    if (!m_map.SetManifest(after, error))
    {
        m_status = "Could not update map: " + error;
        return false;
    }
    RecordManifestEdit(before, after, name);
    m_sceneReloadRequested = true;
    return true;
}

void MapEditorMode::FlushPendingManifestEdit()
{
    m_manifestEdit.Flush(m_undoHistory,
        [this](const auto& manifest) { ApplyManifestSnapshot(manifest); }, "Map parameter edit");
}

void MapEditorMode::FlushPendingGizmoEdit()
{
    if (ImGuizmo::IsUsingAny()) return;
    m_gizmoEdit.Flush(m_undoHistory,
        [this](const auto& manifest) { ApplyManifestSnapshot(manifest); }, "Transform selection");
}

void MapEditorMode::ClearUndoHistory()
{
    m_viewEdit.Clear();
    m_manifestEdit.Clear();
    m_gizmoEdit.Clear();
    m_enemyCatalogEdit.Clear();
    m_undoHistory.Clear();
}

bool MapEditorMode::Undo()
{
    if (!m_map.IsOpen() || ImGuizmo::IsUsingAny()) return false;
    FlushPendingEnemyCatalogEdit();
    FlushPendingGizmoEdit();
    FlushPendingManifestEdit();
    m_viewEdit.Flush();
    if (!m_undoHistory.CanUndo()) return false;
    const std::string name = m_undoHistory.Undo();
    m_status = "Undo: " + name;
    return true;
}

bool MapEditorMode::Redo()
{
    if (!m_map.IsOpen() || ImGuizmo::IsUsingAny()) return false;
    FlushPendingEnemyCatalogEdit();
    FlushPendingGizmoEdit();
    FlushPendingManifestEdit();
    m_viewEdit.Flush();
    if (!m_undoHistory.CanRedo()) return false;
    const std::string name = m_undoHistory.Redo();
    m_status = "Redo: " + name;
    return true;
}

void MapEditorMode::RefreshAssets()
{
    m_roles = {};
    m_inspectedAsset.clear();
    m_roleError.clear();
    if (!Tank::Map::FindMapAssets(m_map.Folder(), m_assets, m_assetError))
    {
        // Do not offer stale files after a failed refresh.
        m_assets.clear();
        m_selectedAsset.clear();
        return;
    }
    if (std::find(m_assets.begin(), m_assets.end(), m_selectedAsset) == m_assets.end())
        m_selectedAsset.clear();
}

void MapEditorMode::Request(Action action)
{
    if (m_pending != Action::None) return;
    FlushPendingEnemyCatalogEdit();
    FlushPendingGizmoEdit();
    FlushPendingManifestEdit();
    m_pending = action;
    m_confirm = m_map.IsDirty();
}

void MapEditorMode::RequestExit()
{
    Request(Action::Exit);
}

void MapEditorMode::RequestApplicationExit()
{
    Request(Action::ExitApplication);
}

bool MapEditorMode::ConsumeSceneReloadRequest()
{
    const bool requested = m_sceneReloadRequested;
    m_sceneReloadRequested = false;
    return requested;
}

bool MapEditorMode::ConsumeFocusSelectedRequest()
{
    const bool requested = m_focusSelectedRequested;
    m_focusSelectedRequested = false;
    return requested;
}

bool MapEditorMode::ConsumeApplicationExitApproval()
{
    const bool approved = m_applicationExitApproved;
    m_applicationExitApproved = false;
    return approved;
}

std::optional<std::filesystem::path> MapEditorMode::ConsumeClosedMapFolder()
{
    std::optional<std::filesystem::path> folder = std::move(m_closedMapFolder);
    m_closedMapFolder.reset();
    return folder;
}

bool MapEditorMode::AddGate(int variant)
{
    if (variant < 0 || variant >= 3) return false;
    const auto size = std::to_string(static_cast<int>(Tank::Map::GateWidths[variant]));
    for (const auto* part : {"Frame", "Shutter"})
    {
        const auto filename = "Gate" + size + part + ".gltf";
        const auto destination = m_map.Folder() / "Models" / filename;
        std::error_code error;
        std::filesystem::create_directories(destination.parent_path(), error);
        if (!error && !std::filesystem::exists(destination, error))
            std::filesystem::copy_file(MapAssetsRoot() / "Templates" / filename, destination,
                std::filesystem::copy_options::none, error);
        if (error) {m_status = "Could not prepare gate asset: " + error.message(); return false;}
        Tank::Map::GltfRoles roles;
        std::string validationError;
        if (!Tank::Map::InspectGltfRoles(destination, roles, validationError) ||
            (m_assetValidator && !m_assetValidator(destination, roles, validationError)))
        {m_status = "Invalid gate asset: " + validationError; return false;}
    }
    const auto before = m_map.Document();
    auto updated = before;
    const auto group = Tank::Map::AddGatePrefab(updated, variant);
    if (!CommitManifestEdit(before, updated, "Add gate")) return false;
    m_selectedNodeId = group;
    m_selectedInstanceId.clear(); m_selectedEnemyId.clear(); m_selectedClearAreaId.clear();
    m_playerStartGizmoSelected = false;
    m_sceneReloadRequested = true; m_focusSelectedRequested = false;
    RefreshAssets();
    m_status = "Added gate. Move the Gate group in Map Tree; Save to write Manifest.json.";
    return true;
}

bool MapEditorMode::AddIndestructibleBox()
{
    // Store a self-contained glTF inside the map, so saved maps remain portable.
    // Static Hit meshes are not registered as destructible combat targets.
    const std::string asset = "Models/IndestructibleBox.gltf";
    const auto destination = m_map.Folder() / asset;
    std::error_code error;
    std::filesystem::create_directories(destination.parent_path(), error);
    if (!error && !std::filesystem::exists(destination, error))
        std::filesystem::copy_file(MapAssetsRoot() / "Templates/IndestructibleBox.gltf",
            destination, std::filesystem::copy_options::none, error);
    if (error)
    {
        m_status = "Could not prepare box asset: " + error.message();
        return false;
    }
    // Never overwrite a user's existing asset. Validate it through the normal path.
    Tank::Map::GltfRoles roles;
    std::string roleError;
    if (!Tank::Map::InspectGltfRoles(destination, roles, roleError))
    {
        m_status = "Could not inspect box asset: " + roleError;
        return false;
    }
    RefreshAssets();
    m_selectedAsset = asset;
    m_inspectedAsset = asset;
    m_roles = std::move(roles);
    m_roleError.clear();
    return AddSelectedModel();
}

bool MapEditorMode::AddSelectedModel()
{
    if (m_selectedAsset != m_inspectedAsset)
    {
        m_status = "Inspect roles before adding this model.";
        return false;
    }
    if (m_assetValidator)
    {
        const std::u8string relativePath(m_selectedAsset.begin(), m_selectedAsset.end());
        std::string error;
        if (!m_assetValidator(m_map.Folder() / std::filesystem::path(relativePath), m_roles, error))
        {
            m_status = "Cannot add model: " + error;
            return false;
        }
    }
    const Tank::Map::Manifest before = m_map.Document();
    Tank::Map::Manifest updated = before;
    Tank::Map::Instance instance;
    instance.id = Tank::Map::MakeUniqueInstanceId(updated);
    instance.asset = m_selectedAsset;
    instance.parentId = SelectedNodeId();
    updated.instances.push_back(std::move(instance));
    if (!CommitManifestEdit(before, updated, "Add model")) return false;
    m_sceneReloadRequested = true;
    m_selectedInstanceId = updated.instances.back().id;
    m_selectedEnemyId.clear();
    m_selectedClearAreaId.clear();
    m_playerStartGizmoSelected = false;
    m_focusSelectedRequested = true;
    m_status = "Added " + m_selectedAsset + ". Save to write Manifest.json.";
    return true;
}

bool MapEditorMode::SaveSelectedMaterialColor()
{
    if (m_selectedAsset.empty() || !m_materialColorLoaded || m_selectedMaterialIndex < 0 ||
        static_cast<size_t>(m_selectedMaterialIndex) >= m_materialColors.size())
    {
        m_status = "Select a material before saving its color.";
        return false;
    }
    const std::u8string relativeUtf8(m_selectedAsset.begin(), m_selectedAsset.end());
    const std::filesystem::path source = m_map.Folder() / std::filesystem::path(relativeUtf8);
    std::filesystem::path destination = source;
    if (!m_materialOverrideFile)
    {
        std::filesystem::path copyName(m_materialCopyName);
        if (copyName.empty() || copyName.filename() != copyName || copyName == "." || copyName == "..")
        {
            m_status = "Enter a file name for the renamed copy.";
            return false;
        }
        if (!copyName.has_extension()) copyName += source.extension();
        if (copyName.extension() != source.extension())
        {
            m_status = "The copy must keep the source file extension.";
            return false;
        }
        destination = source.parent_path() / copyName;
        std::error_code error;
        if (std::filesystem::exists(destination, error) || error)
        {
            m_status = error ? "Could not check copy destination: " + error.message() :
                "A file with that name already exists.";
            return false;
        }
        std::filesystem::copy_file(source, destination, std::filesystem::copy_options::none, error);
        if (error)
        {
            m_status = "Could not copy model: " + error.message();
            return false;
        }
    }

    std::string error;
    if (!Tank::Map::SaveGltfMaterialColor(destination,
        static_cast<size_t>(m_selectedMaterialIndex), m_materialColorEdit, error))
    {
        if (!m_materialOverrideFile)
        {
            std::error_code ignored;
            std::filesystem::remove(destination, ignored);
        }
        m_status = "Could not save material color: " + error;
        return false;
    }

    const auto relative = std::filesystem::relative(destination, m_map.Folder());
    const auto relativeUtf8Result = relative.u8string();
    const std::string updatedAsset(relativeUtf8Result.begin(), relativeUtf8Result.end());
    if (!m_materialOverrideFile)
    {
        m_selectedAsset = updatedAsset;
        m_materialCopyName = destination.stem().string() + "_Color";
    }
    RefreshAssets();
    m_selectedAsset = updatedAsset;
    m_materialColors.clear();
    m_materialColorLoaded = Tank::Map::InspectGltfMaterialColors(destination, m_materialColors, error) &&
        static_cast<size_t>(m_selectedMaterialIndex) < m_materialColors.size();
    if (m_materialColorLoaded)
        m_materialColorEdit = m_materialColors[static_cast<size_t>(m_selectedMaterialIndex)].baseColorFactor;
    if (m_inspectedAsset == updatedAsset) m_inspectedAsset.clear();
    m_sceneReloadRequested = true;
    m_status = std::string(m_materialOverrideFile ? "Updated " : "Saved renamed copy ") + updatedAsset;
    return true;
}

bool MapEditorMode::UpdateSelectedInstance(const Tank::Map::Transform& transform)
{
    const Tank::Map::Manifest before = m_map.Document();
    Tank::Map::Manifest updated = before;
    const auto instance = std::find_if(updated.instances.begin(), updated.instances.end(),
        [this](const Tank::Map::Instance& value) { return value.id == m_selectedInstanceId; });
    if (instance == updated.instances.end())
    {
        m_status = "The selected model no longer exists.";
        m_selectedInstanceId.clear();
        m_focusSelectedRequested = false;
        return false;
    }
    instance->transform = transform;
    return CommitManifestEdit(before, updated, "Move model");
}

void MapEditorMode::SetSelectedInstanceVisible(bool visible)
{
    if (m_selectedInstanceId.empty()) return;
    if (visible)
    {
        m_hiddenInstanceIds.erase(m_selectedInstanceId);
        m_focusSelectedRequested = true;
    }
    else
        m_hiddenInstanceIds.insert(m_selectedInstanceId);
    m_sceneReloadRequested = true;
}

bool MapEditorMode::RemoveSelectedInstance()
{
    const Tank::Map::Manifest before = m_map.Document();
    Tank::Map::Manifest updated = before;
    const auto remove = std::find_if(updated.instances.begin(), updated.instances.end(),
        [this](const Tank::Map::Instance& value) { return value.id == m_selectedInstanceId; });
    if (remove == updated.instances.end()) return false;
    const std::string removedAsset = remove->asset;
    Tank::Map::RemoveInstance(updated, m_selectedInstanceId);
    if (!CommitManifestEdit(before, updated, "Remove model")) return false;
    m_hiddenInstanceIds.erase(m_selectedInstanceId);
    m_selectedInstanceId.clear();
    m_playerStartGizmoSelected = false;
    m_focusSelectedRequested = false;
    m_sceneReloadRequested = true;
    m_status = "Removed " + removedAsset + ". Save to write Manifest.json.";
    return true;
}

bool MapEditorMode::DuplicateSelectedInstance()
{
    const Tank::Map::Manifest before = m_map.Document();
    Tank::Map::Manifest updated = before;
    const std::optional<std::string> duplicateId =
        Tank::Map::DuplicateInstance(updated, m_selectedInstanceId);
    if (!duplicateId) return false;
    if (!CommitManifestEdit(before, updated, "Duplicate model")) return false;
    m_selectedInstanceId = *duplicateId;
    m_sceneReloadRequested = true;
    m_focusSelectedRequested = true;
    m_status = "Duplicated model. Move it, then Save to write Manifest.json.";
    return true;
}

bool MapEditorMode::UpdatePlayerSpawn(const Tank::Map::Transform& transform)
{
    const Tank::Map::Manifest before = m_map.Document();
    Tank::Map::Manifest updated = before;
    updated.playerSpawn = transform;
    return CommitManifestEdit(before, updated, "Move player start");
}

bool MapEditorMode::AddClearArea()
{
    const Tank::Map::Manifest before = m_map.Document();
    Tank::Map::Manifest updated = before;
    const std::string id = Tank::Map::AddClearArea(updated);
    updated.clearAreas.back().parentId = SelectedNodeId();
    if (!CommitManifestEdit(before, updated, "Add clear area")) return false;
    m_selectedClearAreaId = id;
    m_selectedInstanceId.clear();
    m_selectedEnemyId.clear();
    m_playerStartGizmoSelected = false;
    m_sceneReloadRequested = true;
    m_status = "Added clear area. Save to write Manifest.json.";
    return true;
}

bool MapEditorMode::UpdateSelectedClearArea(const Tank::Map::ClearArea& area)
{
    const Tank::Map::Manifest before = m_map.Document();
    Tank::Map::Manifest updated = before;
    const auto selected = std::find_if(updated.clearAreas.begin(), updated.clearAreas.end(),
        [this](const Tank::Map::ClearArea& value) { return value.id == m_selectedClearAreaId; });
    if (selected == updated.clearAreas.end())
    {
        m_status = "The selected clear area no longer exists.";
        m_selectedClearAreaId.clear();
        m_selectedEnemyId.clear();
        return false;
    }
    *selected = area;
    return CommitManifestEdit(before, updated, "Edit clear area");
}

bool MapEditorMode::RemoveSelectedClearArea()
{
    const Tank::Map::Manifest before = m_map.Document();
    Tank::Map::Manifest updated = before;
    if (!Tank::Map::RemoveClearArea(updated, m_selectedClearAreaId)) return false;
    if (!CommitManifestEdit(before, updated, "Remove clear area")) return false;
    m_selectedClearAreaId.clear();
    m_selectedEnemyId.clear();
    m_sceneReloadRequested = true;
    m_status = "Removed clear area. Save to write Manifest.json.";
    return true;
}

bool MapEditorMode::LoadEnemyCatalog()
{
    try
    {
        const std::u8string utf8(m_enemyCatalogPath.begin(), m_enemyCatalogPath.end());
        std::ifstream input(std::filesystem::path(utf8), std::ios::binary);
        if (!input) throw std::runtime_error("Cannot open enemy catalog");
        const std::string text(std::istreambuf_iterator<char>(input), {});
        if (input.bad()) throw std::runtime_error("Cannot read enemy catalog");
        Tank::Physics::EnemyEditorSettings loaded;
        if (!Tank::Physics::DeserializeEnemyEditor(text, loaded, m_enemyCatalogError)) return false;
        std::vector<std::string> names;
        for (const auto& type : loaded.unitTypes) names.push_back(type.name);
        m_enemyTypes = std::move(names);
        m_enemyCatalog = std::move(loaded);
        if (std::find(m_enemyTypes.begin(), m_enemyTypes.end(), m_selectedEnemyType) == m_enemyTypes.end())
            m_selectedEnemyType = m_enemyTypes.empty() ? "" : m_enemyTypes.front();
        m_enemyCatalogError.clear();
        return true;
    }
    catch (const std::exception& exception)
    {
        m_enemyCatalogError = exception.what();
        return false;
    }
}

void MapEditorMode::ApplyEnemyCatalogSnapshot(const Tank::Physics::EnemyEditorSettings& settings)
{
    m_enemyCatalog = settings;
    m_enemyTypes.clear();
    for (const auto& type : m_enemyCatalog.unitTypes) m_enemyTypes.push_back(type.name);
    if (std::find(m_enemyTypes.begin(), m_enemyTypes.end(), m_selectedEnemyType) == m_enemyTypes.end())
        m_selectedEnemyType = m_enemyTypes.empty() ? "" : m_enemyTypes.front();
}

void MapEditorMode::FlushPendingEnemyCatalogEdit()
{
    m_enemyCatalogEdit.Flush(m_undoHistory,
        [this](const auto& settings) { ApplyEnemyCatalogSnapshot(settings); }, "Enemy catalog edit");
}

bool MapEditorMode::AddSelectedEnemy()
{
    const auto before = m_map.Document();
    auto updated = before;
    const auto id = Tank::Map::AddEnemy(updated, m_selectedEnemyType);
    if (id.empty()) return false;
    updated.enemies.back().parentId = SelectedNodeId();
    if (!CommitManifestEdit(before, updated, "Add enemy")) return false;
    m_selectedEnemyId = id;
    m_selectedInstanceId.clear();
    m_selectedClearAreaId.clear();
    m_playerStartGizmoSelected = false;
    m_sceneReloadRequested = true;
    m_focusSelectedRequested = true;
    m_status = "Added enemy. Edit position/rotation in Map Tree, then Save.";
    return true;
}

bool MapEditorMode::UpdateSelectedEnemy(const Tank::Map::EnemyPlacement& enemy)
{
    const auto before = m_map.Document();
    auto updated = before;
    const auto selected = std::find_if(updated.enemies.begin(), updated.enemies.end(),
        [this](const auto& value) { return value.id == m_selectedEnemyId; });
    if (selected == updated.enemies.end()) return false;
    *selected = enemy;
    return CommitManifestEdit(before, updated, "Edit enemy");
}

bool MapEditorMode::RemoveSelectedEnemy()
{
    const auto before = m_map.Document();
    auto updated = before;
    if (!Tank::Map::RemoveEnemy(updated, m_selectedEnemyId)) return false;
    if (!CommitManifestEdit(before, updated, "Remove enemy")) return false;
    m_selectedEnemyId.clear();
    m_playerStartGizmoSelected = false;
    m_sceneReloadRequested = true;
    m_focusSelectedRequested = false;
    m_status = "Removed enemy. Save to write Manifest.json.";
    return true;
}

bool MapEditorMode::PlacementGizmoCapturesMouse() const
{
    const bool hasSelection = !m_selectedEnemyId.empty() || !m_selectedInstanceId.empty() ||
        !m_selectedClearAreaId.empty() || m_playerStartGizmoSelected;
    return m_gizmoEnabled && hasSelection &&
        (ImGuizmo::IsOver() || ImGuizmo::IsUsingAny());
}

std::string MapEditorMode::SelectedNodeId() const
{
    std::string id = m_selectedNodeId;
    if (!m_selectedInstanceId.empty()) id = "mesh/" + m_selectedInstanceId;
    else if (!m_selectedEnemyId.empty()) id = "enemy/" + m_selectedEnemyId;
    else if (!m_selectedClearAreaId.empty()) id = "area/" + m_selectedClearAreaId;
    else if (m_playerStartGizmoSelected)
        id = m_selectedNodeId.starts_with("start/") ? m_selectedNodeId : "player-start";
    const auto tree = Tank::Map::MapTree(m_map.Document());
    return std::any_of(tree.begin(), tree.end(), [&](const auto& n) {return n.id == id;}) ? id : "root";
}
void MapEditorMode::DrawPlacementGizmo(const Engine::CameraState& camera, float aspectRatio)
{
    if (!m_map.IsOpen() || !m_gizmoEnabled || m_confirm || m_pending != Action::None)
    { FlushPendingGizmoEdit(); ImGuizmo::Enable(false); return; }
    const auto& document = m_map.Document();
    FlushPendingGizmoEdit();
    const auto id = SelectedNodeId();
    const auto tree = Tank::Map::MapTree(document);
    const auto selected = std::find_if(tree.begin(), tree.end(), [&](const auto& n) {return n.id == id;});
    if (selected == tree.end()) { ImGuizmo::Enable(false); return; }
    const bool rigid = selected->kind == Tank::Map::NodeKind::Enemy || selected->kind == Tank::Map::NodeKind::PlayerStartPoint;
    if (rigid && m_gizmoOperation == GizmoOperation::Scale) m_gizmoOperation = GizmoOperation::Translate;
    ImGuizmo::Enable(ImGuizmo::IsUsingAny() || !ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow));
    ImGuizmo::SetDrawlist(ImGui::GetBackgroundDrawList());
    const auto* viewport = ImGui::GetMainViewport();
    ImGuizmo::SetRect(viewport->Pos.x, viewport->Pos.y, viewport->Size.x, viewport->Size.y);
    ImGuizmo::SetOrthographic(camera.projection == Engine::CameraProjection::Orthographic);
    DirectX::XMFLOAT4X4 view, projection;
    DirectX::XMStoreFloat4x4(&view, Engine::CreateCameraViewMatrix(camera));
    DirectX::XMStoreFloat4x4(&projection, Engine::CreateCameraProjectionMatrix(camera, aspectRatio));
    auto world = Tank::Map::NodeWorldTransform(document, id);
    const auto operation = m_gizmoOperation == GizmoOperation::Rotate ? ImGuizmo::ROTATE :
        m_gizmoOperation == GizmoOperation::Scale ? ImGuizmo::SCALE : ImGuizmo::TRANSLATE;
    if (ImGuizmo::Manipulate(&view._11, &projection._11, operation,
        m_gizmoLocal ? ImGuizmo::LOCAL : ImGuizmo::WORLD, world.worldMatrix->data()))
    {
        if ((m_gridSnapEnabled || ImGui::GetIO().KeyShift) && m_gizmoOperation == GizmoOperation::Translate)
        {
            auto& matrix = *world.worldMatrix;
            const auto snapped = Tank::Map::SnapPositionToGrid({matrix[12], matrix[13], matrix[14]}, m_gridSpacingMeters);
            std::copy(snapped.begin(), snapped.end(), matrix.begin() + 12);
        }
        Tank::Map::Transform local;
        std::string error;
        const auto before = document;
        auto updated = before;
        if (Tank::Map::NodeTransformFromWorld(document, id, *world.worldMatrix, local, error) &&
            Tank::Map::SetNodeLocalTransform(updated, id, local))
        {
            FlushPendingEnemyCatalogEdit();
            FlushPendingManifestEdit();
            if (m_map.SetManifest(updated, error))
            {
                m_viewEdit.Flush();
                m_gizmoEdit.Update(before, updated, ImGuizmo::IsUsingAny() ? 1 : 0,
                    m_undoHistory, [this](const auto& manifest) { ApplyManifestSnapshot(manifest); },
                    "Transform selection");
                m_sceneReloadRequested = true;
            }
            else
                m_status = "Could not transform node: " + error;
        }
        else
        {
            m_status = "Could not transform node: " + error;
        }
    }
    FlushPendingGizmoEdit();
}
void MapEditorMode::UpdateViewportPicking(
    const std::function<std::optional<std::string>(float, float)>& pick)
{
    const auto& io = ImGui::GetIO();
    const bool blocked = !m_map.IsOpen() || m_confirm || m_pending != Action::None ||
        io.WantCaptureMouse || ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow) ||
        ImGuizmo::IsOver() || ImGuizmo::IsUsingAny();
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) m_viewportPickPending = !blocked;
    if (blocked) m_viewportPickPending = false;
    if (!ImGui::IsMouseReleased(ImGuiMouseButton_Left)) return;
    const bool click = std::exchange(m_viewportPickPending, false) &&
        io.MouseDragMaxDistanceSqr[ImGuiMouseButton_Left] <= io.MouseDragThreshold * io.MouseDragThreshold;
    if (!click) return;
    const auto* viewport = ImGui::GetMainViewport();
    const auto picked = pick(io.MousePos.x - viewport->Pos.x, io.MousePos.y - viewport->Pos.y);
    if (!picked) return;
    const auto tree = Tank::Map::MapTree(m_map.Document());
    const auto node = std::find_if(tree.begin(), tree.end(), [&](const auto& n) {return n.id == *picked;});
    if (node == tree.end()) return;
    m_selectedNodeId = node->id;
    m_selectedInstanceId = node->kind == Tank::Map::NodeKind::Mesh ? node->objectId : "";
    m_selectedEnemyId = node->kind == Tank::Map::NodeKind::Enemy ? node->objectId : "";
    m_selectedClearAreaId = node->kind == Tank::Map::NodeKind::ClearArea ? node->objectId : "";
    m_playerStartGizmoSelected = node->kind == Tank::Map::NodeKind::PlayerStartPoint;
    m_revealTreeSelection = true;
    m_sceneReloadRequested = true;
}

void MapEditorMode::DrawMapTree()
{
    using namespace Tank::Map;
    ImGui::SeparatorText("Map Tree");
    auto updated = m_map.Document();
    const auto tree = MapTree(updated);
    bool changed = false;
    std::string selectedId = SelectedNodeId();
    const auto select = [&](const TreeNode& node)
    {
        m_selectedNodeId = node.id;
        selectedId = node.id;
        m_selectedInstanceId = node.kind == NodeKind::Mesh ? node.objectId : "";
        m_selectedEnemyId = node.kind == NodeKind::Enemy ? node.objectId : "";
        m_selectedClearAreaId = node.kind == NodeKind::ClearArea ? node.objectId : "";
        m_playerStartGizmoSelected = node.kind == NodeKind::PlayerStartPoint;
        m_sceneReloadRequested = true;
    };
    std::vector<Tank::Ui::SceneTreeItem> items;
    static constexpr const char* kinds[] = {"Empty", "glTF", "PlayerStartPoint", "Enemy", "ClearArea"};
    for (const auto& node : tree)
        items.push_back({node.id, node.parentId, node.name + " [" + kinds[static_cast<int>(node.kind)] + "]" +
            (NodeIsEnabled(updated, node.id) ? "" : " [Inactive]") +
            (NodeIsEnabled(updated, node.id, true) ? "" : " [Hidden]")});
    const auto placement = [](Tank::Ui::SceneTreeDropPosition position)
    {
        switch (position)
        {
        case Tank::Ui::SceneTreeDropPosition::Before: return NodePlacement::Before;
        case Tank::Ui::SceneTreeDropPosition::After: return NodePlacement::After;
        default: return NodePlacement::Child;
        }
    };
    const auto result = Tank::Ui::SceneTreeUI("MapSceneTree", items, "root", selectedId,
        [&](const Tank::Ui::SceneTreeDrop& drop)
        {
            auto probe = updated;
            std::string error;
            return MoveNode(probe, drop.sourceId, drop.targetId, placement(drop.position), error);
        }, std::exchange(m_revealTreeSelection, false));
    if (result.selectedId)
        for (const auto& node : tree) if (node.id == *result.selectedId) select(node);
    const bool contextDuplicate = result.command && result.command->action == Tank::Ui::SceneTreeAction::Duplicate;
    const bool contextDelete = result.command && result.command->action == Tank::Ui::SceneTreeAction::Delete;
    if (result.command && result.command->action == Tank::Ui::SceneTreeAction::Rename)
    {
        m_renameNodeId = result.command->nodeId;
        for (const auto& node : tree) if (node.id == m_renameNodeId) m_renameNodeName = node.name;
        m_renameNodeError.clear();
        ImGui::OpenPopup("Rename Tree Element");
    }
    if (ImGui::BeginPopupModal("Rename Tree Element", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
        const bool enter = ImGui::InputText("Name", &m_renameNodeName, ImGuiInputTextFlags_EnterReturnsTrue);
        if (!m_renameNodeError.empty()) ImGui::TextWrapped("%s", m_renameNodeError.c_str());
        if (ImGui::Button("Rename") || enter)
        {
            if (RenameNode(updated, m_renameNodeId, m_renameNodeName, m_renameNodeError))
            { changed = true; ImGui::CloseCurrentPopup(); }
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel") || ImGui::IsKeyPressed(ImGuiKey_Escape)) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
    if (result.drop)
    {
        std::string error;
        if (MoveNode(updated, result.drop->sourceId, result.drop->targetId, placement(result.drop->position), error))
            changed = true;
        else m_status = error;
    }
    if (ImGui::Button("Add Empty Node"))
    {
        std::string id;
        for (int i = 1;; ++i)
        {
            id = "node-" + std::to_string(i);
            if (std::none_of(tree.begin(), tree.end(), [&](const auto& n) {return n.id == id;})) break;
        }
        const bool validSelection = std::any_of(tree.begin(), tree.end(), [&](const auto& n) {return n.id == selectedId;});
        updated.nodes.push_back({id, "Empty", validSelection ? selectedId : "root", {}});
        select({id, "Empty", validSelection ? selectedId : "root", NodeKind::Empty, id});
        changed = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Add Clear Area")) AddClearArea();
    const auto selected = std::find_if(tree.begin(), tree.end(), [&](const auto& n) {return n.id == selectedId;});
    if (selected != tree.end())
    {
        ImGui::Text("Selected: %s", selected->name.c_str());
        auto info = GetNodeInfo(updated, selectedId);
        ImGui::BeginDisabled(selectedId == "root");
        if (ImGui::Checkbox("Active", &info.active))
        {
            std::string error;
            if (SetNodeActive(updated, selectedId, info.active, error)) changed = true;
            else m_status = error;
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        if (ImGui::Checkbox("Visible in editor", &info.visible))
        {
            auto current = GetNodeInfo(updated, selectedId);
            current.visible = info.visible;
            updated.nodeInfo[selectedId] = current;
            changed = true;
        }
        for (auto& model : updated.instances)
            if (model.gate && ("mesh/" + model.id == selectedId || model.parentId == selectedId))
            {
                ImGui::PushID(model.id.c_str());
                if (ImGui::CollapsingHeader("Gate proximity trigger", ImGuiTreeNodeFlags_DefaultOpen))
                {
                    changed |= ImGuiWidgets::SliderFloatWithControls("Open radius (m)", &model.gate->triggerRadiusMeters,
                        0.5f, 1000, 0.5f, 12, "%.1f", ImGuiSliderFlags_AlwaysClamp);
                    ImGui::TextWrapped("Starts closed. Opens when the player center enters this sphere; stays open until replay.");
                    ImGui::TextDisabled("Radius center: shutter base. Move/rotate the Gate group to keep the frame aligned.");
                }
                ImGui::PopID();
            }
        if (selected->kind == NodeKind::Enemy)
        {
            auto enemy = std::find_if(updated.enemies.begin(), updated.enemies.end(),
                [&](const auto& n) {return n.id == selected->objectId;});
            if (enemy != updated.enemies.end())
                changed |= ImGui::Checkbox("Supported by parent", &enemy->supportedByParent);
            ImGui::TextDisabled("Falls when its nearest Breakable ancestor is destroyed.");
        }
        if (selected->kind == NodeKind::Mesh)
        {
            auto model = std::find_if(updated.instances.begin(), updated.instances.end(),
                [&](const auto& n) {return n.id == selected->objectId;});
            if (model != updated.instances.end())
            {
                ImGui::BeginDisabled(model->gate.has_value());
                changed |= ImGui::Checkbox("Breakable", &model->breakable);
                ImGui::BeginDisabled(!model->breakable);
                changed |= ImGuiWidgets::SliderFloatWithControls("HP", &model->hitPoints,
                    1, 100000, 10, 60, "%.0f");
                model->hitPoints = std::clamp(model->hitPoints, 1.0f, 100000.0f);
                ImGui::EndDisabled();
                ImGui::EndDisabled();
            }
        }
        if (selected->kind == NodeKind::PlayerStartPoint)
            ImGui::TextWrapped("Position is the tank chassis center. Keep Y above the HitMesh.");
        if (selectedId != "root" && ImGui::BeginCombo("Parent", selected->parentId.c_str()))
        {
            for (const auto& candidate : tree)
            {
                auto probe = updated;
                std::string error;
                if (!MoveNode(probe, selectedId, candidate.id, NodePlacement::Child, error)) continue;
                ImGui::PushID(candidate.id.c_str());
                if (ImGui::Selectable(candidate.name.c_str(), candidate.id == selected->parentId))
                { updated = std::move(probe); changed = true; }
                ImGui::PopID();
            }
            ImGui::EndCombo();
        }
        ImGui::TextDisabled("Transforms are relative to the parent. Reparenting keeps local values.");
        auto transform = NodeLocalTransform(updated, selectedId);
        static constexpr float zero[3] = {0, 0, 0};
        static constexpr float one[3] = {1, 1, 1};
        ImGui::PushID("TreeInspector");
        ImGui::TextUnformatted("Position (m) - Local");
        bool poseChanged = ImGuiWidgets::SliderFloat3AxisWithControls("Position", transform.position.data(),
            -10000, 10000, m_gridSnapEnabled ? m_gridSpacingMeters : 0.05f, zero, "%.2f");
        if (poseChanged && m_gridSnapEnabled)
        {
            auto probe = updated;
            if (SetNodeLocalTransform(probe, selectedId, transform))
            {
                auto world = NodeWorldTransform(probe, selectedId);
                auto& matrix = *world.worldMatrix;
                const auto snapped = SnapPositionToGrid({matrix[12], matrix[13], matrix[14]}, m_gridSpacingMeters);
                std::copy(snapped.begin(), snapped.end(), matrix.begin() + 12);
                std::string error;
                if (!NodeTransformFromWorld(probe, selectedId, matrix, transform, error))
                    m_status = "Could not snap position: " + error;
            }
        }
        {
            ImGui::TextUnformatted("Rotation (deg) - Local");
            poseChanged |= ImGuiWidgets::SliderFloat3AxisWithControls("Rotation", transform.rotationDegrees.data(),
                -360, 360, 1, zero, "%.1f");
        }
        if (selected->kind != NodeKind::Enemy && selected->kind != NodeKind::PlayerStartPoint)
        {
            ImGui::TextUnformatted("Scale - Local");
            poseChanged |= ImGuiWidgets::SliderFloat3AxisWithControls("Scale", transform.scale.data(),
                0.01f, 100, 0.01f, one, "%.2f");
        }
        ImGui::PopID();
        if (poseChanged) changed |= SetNodeLocalTransform(updated, selectedId, transform);
        auto nodeName = selected->name;
        if (ImGui::InputText("Node Name", &nodeName))
        {
            std::string error;
            if (RenameNode(updated, selectedId, nodeName, error)) changed = true;
            else m_status = error;
        }
        ImGui::BeginDisabled(selectedId == "root");
        if (ImGui::Button("Duplicate") || contextDuplicate)
        {
            std::string error;
            if (const auto duplicate = DuplicateNode(updated, selectedId, error))
            {
                for (const auto& n : MapTree(updated)) if (n.id == *duplicate) select(n);
                changed = true;
            }
            else m_status = error;
        }
        ImGui::SameLine();
        if (ImGui::Button("Remove Selected") || contextDelete)
        {
            std::string error;
            if (RemoveNode(updated, selectedId, error))
            { select({"root", "Root", "", NodeKind::Empty, "root"}); changed = true; }
            else m_status = error;
        }
        ImGui::EndDisabled();
    }
    if (changed)
    {
        FlushPendingEnemyCatalogEdit();
        FlushPendingGizmoEdit();
        const auto before = m_map.Document();
        std::string error;
        if (m_map.SetManifest(updated, error))
        {
            m_viewEdit.Flush();
            m_manifestEdit.Update(before, updated, ImGui::GetActiveID(), m_undoHistory,
                [this](const auto& manifest) { ApplyManifestSnapshot(manifest); }, "Map parameter edit");
            m_sceneReloadRequested = true;
        }
        else m_status = "Could not update map tree: " + error;
    }
    m_viewEdit.FinishInteraction(ImGui::GetActiveID());
    m_manifestEdit.FinishInteraction(ImGui::GetActiveID(), m_undoHistory,
        [this](const auto& manifest) { ApplyManifestSnapshot(manifest); }, "Map parameter edit");
}

void MapEditorMode::DrawEnemies()
{
    if (!m_enemyCatalogInitialized)
    {
        m_enemyCatalogInitialized = true;
        m_enemyCatalogPath = TANK_SOURCE_CONFIG_DIR "/enemy_types.json";
        LoadEnemyCatalog();
    }
    ImGui::SeparatorText("Enemies");
    ImGui::TextWrapped("Enemy catalog: %s", m_enemyCatalogPath.c_str());
    if (!m_enemyCatalogError.empty())
        ImGui::TextWrapped("Catalog load failed (previous types retained): %s", m_enemyCatalogError.c_str());
    auto typeCombo = [this](const char* label, std::string& name)
    {
        bool changed = false;
        if (ImGui::BeginCombo(label, name.c_str()))
        {
            for (size_t i = 0; i < m_enemyTypes.size(); ++i)
            {
                ImGui::PushID(static_cast<int>(i));
                // Render names literally, including ImGui label delimiters.
                const auto pos = ImGui::GetCursorScreenPos();
                if (ImGui::Selectable("##type", name == m_enemyTypes[i]))
                { name = m_enemyTypes[i]; changed = true; }
                ImGui::GetWindowDrawList()->AddText(pos, ImGui::GetColorU32(ImGuiCol_Text), m_enemyTypes[i].c_str());
                ImGui::PopID();
            }
            ImGui::EndCombo();
        }
        return changed;
    };
    if (ImGui::CollapsingHeader("New Enemy", ImGuiTreeNodeFlags_DefaultOpen))
    {
        typeCombo("Placement type", m_selectedEnemyType);
        ImGui::BeginDisabled(m_selectedEnemyType.empty());
        if (ImGui::Button("Add Enemy")) AddSelectedEnemy();
        ImGui::EndDisabled();
        ImGui::TextWrapped("Red boxes and forward markers show placements. Dimensions are preview placeholders.");
    }
    if (!ImGui::CollapsingHeader("Selected Enemy", ImGuiTreeNodeFlags_DefaultOpen)) return;
    const auto& enemies = m_map.Document().enemies;
    const auto selected = std::find_if(enemies.begin(), enemies.end(),
        [this](const auto& value) { return value.id == m_selectedEnemyId; });
    if (selected != enemies.end())
    {
        auto enemy = *selected;
        if (std::find(m_enemyTypes.begin(), m_enemyTypes.end(), enemy.unitType) == m_enemyTypes.end())
            ImGui::TextWrapped("Type is missing from loaded catalog: %s", enemy.unitType.c_str());
        ImGui::Text("Enemy ID: %s", enemy.id.c_str());
        const bool changed = typeCombo("Enemy type", enemy.unitType);
        if (changed) UpdateSelectedEnemy(enemy);
        if (ImGui::Button("Focus Enemy")) { m_sceneReloadRequested = true; m_focusSelectedRequested = true; }
    }
    else
        ImGui::TextUnformatted("Select an enemy in the map to edit its type.");
}

bool MapEditorMode::Save()
{
    FlushPendingEnemyCatalogEdit();
    FlushPendingGizmoEdit();
    FlushPendingManifestEdit();
    std::string error;
    std::vector<Tank::Map::NamedMap> maps;
    if (!Tank::Map::LoadMapCatalog(MapAssetsRoot(), maps, error) ||
        !Tank::Map::IsMapNameAvailable(maps, m_map.Folder(), m_map.Document().name, error) ||
        !m_map.Save(error))
    {
        m_status = "Save failed: " + error;
        return false;
    }
    m_status = "Saved Manifest.json";
    RefreshAvailableMaps();
    return true;
}

void MapEditorMode::DrawCheatSheet()
{
    if (!m_showCheatSheet) return;

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(
        ImVec2(viewport->WorkPos.x + viewport->WorkSize.x - 330.0f, viewport->WorkPos.y + 10.0f),
        ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(320.0f, 410.0f), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Map Editor Cheat Sheet", &m_showCheatSheet, ImGuiWindowFlags_NoCollapse))
    {
        ImGui::SeparatorText("Camera");
        ImGui::BulletText("Left click: Select visible object");
        ImGui::BulletText("Left drag: Orbit");
        ImGui::BulletText("Shift + Move gizmo: Grid snap");
        ImGui::BulletText("Middle drag: Pan");
        ImGui::BulletText("Mouse wheel: Zoom");
        ImGui::TextWrapped("Move the pointer outside GUI windows before operating the camera.");

        ImGui::SeparatorText("Add a Model");
        ImGui::BulletText("Open a map folder");
        ImGui::BulletText("Select a glTF or GLB file");
        ImGui::BulletText("Inspect Roles");
        ImGui::BulletText("Add to Map");

        ImGui::SeparatorText("Edit the Map");
        ImGui::BulletText("Map Tree: model position, rotation and scale");
        ImGui::BulletText("Selected model: yellow highlight box");
        ImGui::BulletText("Map Tree: Active, Visible in editor, Duplicate and Remove Selected");
        ImGui::BulletText("Show Visual / Hit Meshes and Hit Fallback Color: preview display settings");
        ImGui::BulletText("Missing Visual Mesh: warning and Hit fallback");
        ImGui::BulletText("Map Tree: select PlayerStartPoint or Enemy to edit position/rotation");
        ImGui::TextWrapped("Place Player Start Y above the HitMesh so the tank does not spawn inside the ground.");
        ImGui::BulletText("Clear Areas: 1 m unit box, edited through Tree transforms");
        ImGui::BulletText("Select a model, enemy, clear area, or player start; drag the Move/Rotate/Scale gizmo");
        ImGui::BulletText("Local: unit axes; unchecked: world axes");
        ImGui::BulletText("Save: write changes to Manifest.json");

        ImGui::SeparatorText("Preview Markers");
        ImGui::BulletText("Red / green lines: X / Z axes");
        ImGui::BulletText("Orange arrow: player start and forward direction");
        ImGui::BulletText("Cyan wire box: clear area");

        ImGui::SeparatorText("Navigation");
        ImGui::BulletText("ESC: Back to Menu");
    }
    ImGui::End();
}

bool MapEditorMode::Execute(HWND__* owner)
{
    const Action action = m_pending;
    m_pending = Action::None;
    if (action == Action::ExitApplication)
    {
        m_applicationExitApproved = true;
        return false;
    }
    if (action == Action::Exit)
    {
        ClearUndoHistory();
        if (m_map.IsOpen()) m_closedMapFolder = m_map.Folder();
        m_map = {};
        m_status.clear();
        m_previewWarning.clear();
        m_assets.clear();
        m_availableMaps.clear();
        m_selectedAvailableMap.clear();
        m_availableMapsScanned = false;
        m_selectedAsset.clear();
        m_assetError.clear();
        m_roles = {};
        m_inspectedAsset.clear();
        m_roleError.clear();
        m_selectedInstanceId.clear();
        m_focusSelectedRequested = false;
        m_hiddenInstanceIds.clear();
        m_selectedClearAreaId.clear();
        m_selectedEnemyId.clear();
        m_sceneReloadRequested = true;
        return true;
    }
    if (action == Action::OpenSelected)
    {
        std::string error;
        if (!OpenMapFolder(m_pendingFolder, error)) m_status = "Open failed: " + error;
        m_pendingFolder.clear();
    }
    if (action == Action::Open || action == Action::Create)
    {
        std::filesystem::path selected;
        std::string error;
        const auto result = Tank::Platform::Windows::PickMapFolder(owner, MapAssetsRoot(), selected, error,
            action == Action::Create ? L"Create New Map Folder" : L"Open Map Folder");
        if (result == Tank::Platform::Windows::FolderPickerResult::Selected)
        {
            if (OpenMapFolder(selected, error))
                m_status = action == Action::Create ? "Created or opened Manifest.json" : m_status;
            else m_status = "Open failed: " + error;
        }
        else if (result == Tank::Platform::Windows::FolderPickerResult::Failed)
            m_status = error;
    }
    return false;
}

bool MapEditorMode::DrawUi(HWND__* owner, bool* outputWindowVisible)
{
    const auto* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + 10, viewport->WorkPos.y + 10), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(520, 700), ImGuiCond_FirstUseEver);
    ImGui::Begin("Map Editor", nullptr, ImGuiWindowFlags_NoCollapse);
    m_viewEdit.FinishInteraction(ImGui::GetActiveID());
    m_manifestEdit.FinishInteraction(ImGui::GetActiveID(), m_undoHistory,
        [this](const auto& manifest) { ApplyManifestSnapshot(manifest); }, "Map parameter edit");
    m_enemyCatalogEdit.FinishInteraction(ImGui::GetActiveID(), m_undoHistory,
        [this](const auto& settings) { ApplyEnemyCatalogSnapshot(settings); }, "Enemy catalog edit");
    const auto& io = ImGui::GetIO();
    if (!m_confirm && !io.WantTextInput && !ImGui::IsAnyItemActive() &&
        !ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel) && io.KeyCtrl &&
        !io.KeyAlt && !io.KeySuper)
    {
        if (ImGui::IsKeyPressed(ImGuiKey_Y, false) || (io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_Z, false))) Redo();
        else if (!io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_Z, false)) Undo();
    }
    if (!m_availableMapsScanned) RefreshAvailableMaps();
    if (ImGui::Button("Open Folder...")) Request(Action::Open);
    ImGui::SameLine();
    if (ImGui::Button("Create New...")) Request(Action::Create);
    ImGui::SameLine();
    ImGui::BeginDisabled(!m_map.IsOpen());
    if (ImGui::Button("Save")) Save();
    ImGui::EndDisabled();
    ImGui::BeginDisabled(!CanUndo());
    if (ImGui::Button("Undo")) Undo();
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(!CanRedo());
    if (ImGui::Button("Redo")) Redo();
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) ImGui::SetTooltip("Ctrl+Y / Ctrl+Shift+Z");
    ImGui::SameLine();
    if (ImGui::Button("Back to Menu")) RequestExit();
    auto viewFields = [this]()
    {
        return std::tie(m_showGrid, m_gridSnapEnabled, m_gridSpacingMeters, m_gridHalfCellCount,
            m_gridLineWidthMeters, m_showVisualMeshes, m_showHitMeshes, m_visualMeshColor,
            m_gizmoEnabled, m_gizmoLocal, m_gizmoOperation, m_showCameraWindow, m_showCheatSheet);
    };
    const auto beforeView = std::apply([](const auto&... value) { return std::make_tuple(value...); }, viewFields());
    if (ImGui::CollapsingHeader("Sub Window", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::BeginDisabled(!m_map.IsOpen());
        if (ImGui::Checkbox("Enemy Unit Editor", &m_enemyUnitEditor.open) && m_enemyUnitEditor.open)
        {
            ImGui::SetWindowCollapsed("Enemy Unit Editor", false);
            ImGui::SetWindowFocus("Enemy Unit Editor");
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::Checkbox("Camera Window", &m_showCameraWindow);
        ImGui::Checkbox("Show Cheat Sheet", &m_showCheatSheet);
        if (outputWindowVisible)
        {
            ImGui::SameLine();
            ImGui::Checkbox("Output (F9)", outputWindowVisible);
        }
    }
    ImGui::Checkbox("Placement Gizmo", &m_gizmoEnabled);
    ImGui::SameLine();
    if (ImGui::RadioButton("Move", m_gizmoOperation == GizmoOperation::Translate))
        m_gizmoOperation = GizmoOperation::Translate;
    ImGui::SameLine();
    if (ImGui::RadioButton("Rotate", m_gizmoOperation == GizmoOperation::Rotate))
        m_gizmoOperation = GizmoOperation::Rotate;
    ImGui::SameLine();
    const bool canScaleSelection = !m_selectedInstanceId.empty() || !m_selectedClearAreaId.empty() ||
        (m_selectedEnemyId.empty() && !m_playerStartGizmoSelected);
    ImGui::BeginDisabled(!canScaleSelection);
    if (ImGui::RadioButton("Scale", m_gizmoOperation == GizmoOperation::Scale))
        m_gizmoOperation = GizmoOperation::Scale;
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::Checkbox("Local", &m_gizmoLocal);
    ImGui::Separator();
    ImGui::BeginChild("##MapEditorScrollArea", ImVec2(0.0f, 0.0f), false);
    if (ImGui::CollapsingHeader("Map List", ImGuiTreeNodeFlags_DefaultOpen))
    {
        char mapFolder[] = "Assets/Map";
        ImGui::BeginDisabled();
        ImGui::TextUnformatted("MapFolder:");
        ImGui::SameLine();
        ImGui::InputText("##MapFolder", mapFolder, sizeof(mapFolder), ImGuiInputTextFlags_ReadOnly);
        ImGui::EndDisabled();
        if (ImGui::Button("Refresh Map List")) RefreshAvailableMaps();
        if (Ui::BeginMapListTable("##AvailableMaps", ImGui::GetTextLineHeightWithSpacing() * 6.0f))
        {
            for (const auto& map : m_availableMaps)
            {
                const auto& document = map.folder == m_map.Folder() ? m_map.Document() : map.document;
                if (Ui::DrawMapListRow(document, map.folder, map.folder == m_selectedAvailableMap))
                    m_selectedAvailableMap = map.folder;
            }
            ImGui::EndTable();
        }
        ImGui::BeginDisabled(m_selectedAvailableMap.empty());
        if (ImGui::Button("Open Selected"))
        {
            m_pendingFolder = m_selectedAvailableMap;
            Request(Action::OpenSelected);
        }
        ImGui::EndDisabled();
        if (m_availableMaps.empty()) ImGui::TextUnformatted("No map folders with Manifest.json found in Assets/Map.");
    }
    ImGui::Separator();
    if (!m_map.IsOpen())
    {
        ImGui::TextWrapped("Choose a map above, or use Create New... to create a folder and Manifest.json.");
    }
    else
    {
        const auto utf8 = m_map.Folder().u8string();
        const std::string folder(utf8.begin(), utf8.end());
        ImGui::TextWrapped("Folder: %s", folder.c_str());
        const bool rename = ImGui::InputText("Map Name", &m_mapNameDraft, ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::SameLine();
        const bool applyName = ImGui::Button("Apply Name");
        if (rename || applyName)
        {
            std::vector<Tank::Map::NamedMap> maps;
            if (Tank::Map::LoadMapCatalog(MapAssetsRoot(), maps, m_mapNameError) &&
                Tank::Map::IsMapNameAvailable(maps, m_map.Folder(), m_mapNameDraft, m_mapNameError))
            {
                const auto before = m_map.Document();
                auto after = before;
                after.name = m_mapNameDraft;
                if (CommitManifestEdit(before, after, "Rename map"))
                    m_status = "Map name changed. Save to update name references on disk.";
            }
        }
        ImGui::TextDisabled("Enter / Apply Name, then Save. Map references use this name (case-sensitive).");
        if (!m_mapNameError.empty()) ImGui::TextWrapped("%s", m_mapNameError.c_str());
        bool playable = m_map.Document().playable;
        if (ImGui::Checkbox("Playable", &playable))
        {
            const auto before = m_map.Document();
            auto after = before;
            after.playable = playable;
            CommitManifestEdit(before, after, "Set map playable");
        }
        ImGui::TextUnformatted(m_map.IsDirty() ? "Unsaved changes" : "Saved");
        DrawMapTree();
        const auto& document = m_map.Document();
        ImGui::Text("Models: %zu", document.instances.size());
        ImGui::Text("Clear areas: %zu", document.clearAreas.size());
        ImGui::SeparatorText("Grid");
        if (ImGui::Checkbox("Show Grid", &m_showGrid)) m_sceneReloadRequested = true;
        ImGui::Checkbox("Grid Snap", &m_gridSnapEnabled);
        ImGui::TextDisabled("Snap movement in world XYZ using Spacing (m).");
        bool gridChanged = ImGuiWidgets::SliderFloatWithControls(
            "Spacing (m)", &m_gridSpacingMeters, 0.1f, 100.0f, 0.05f, 1.0f, "%.2f");
        gridChanged |= ImGuiWidgets::SliderIntWithControls(
            "Half cells", &m_gridHalfCellCount, 1, 100, 1, 10);
        gridChanged |= ImGuiWidgets::SliderFloatWithControls(
            "Line width (m)", &m_gridLineWidthMeters, 0.005f, 0.2f, 0.005f, 0.02f, "%.3f");
        if (gridChanged) m_sceneReloadRequested = true;
        ImGui::SeparatorText("Mesh Display");
        bool meshDisplayChanged = ImGui::Checkbox("Show Visual Meshes", &m_showVisualMeshes);
        ImGui::SameLine();
        meshDisplayChanged |= ImGui::Checkbox("Show Hit Meshes", &m_showHitMeshes);
        static constexpr float defaultVisualMeshColor[3] = {
            80.0f / 255.0f, 80.0f / 255.0f, 80.0f / 255.0f };
        static constexpr const char* colorChannels[3] = { "R", "G", "B" };
        ImGui::TextUnformatted("Hit Fallback Color");
        meshDisplayChanged |= ImGuiWidgets::SliderFloat3AxisWithControls(
            "Hit Fallback Color", m_visualMeshColor.data(), 0.0f, 1.0f, 0.01f,
            defaultVisualMeshColor, "%.2f", 0, colorChannels);
        if (meshDisplayChanged) m_sceneReloadRequested = true;
        ImGui::TextDisabled("Display only. Hit collision remains active during play.");
        if (!m_previewWarning.empty())
        {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.72f, 0.2f, 1.0f));
            ImGui::TextWrapped("Warning: %s", m_previewWarning.c_str());
            ImGui::PopStyleColor();
        }
        ImGui::Separator();
        if (ImGui::CollapsingHeader("Gate", ImGuiTreeNodeFlags_DefaultOpen))
        {
            const int previousVariant = m_gateVariant;
            ImGui::Combo("Opening (W x H)", &m_gateVariant, "10m x 3m\0" "5m x 3m\0" "20m x 5m\0");
            ObserveViewEdit(previousVariant, m_gateVariant, ImGui::GetActiveID(),
                [this](int value) {m_gateVariant = value;}, "Gate placement size");
            if (ImGui::Button("Add Gate")) AddGate(m_gateVariant);
            ImGui::TextWrapped("Framed vertical shutter. Select the Gate group or shutter in Map Tree to set its open radius.");
        }
        if (ImGui::Button("Add Indestructible Box")) AddIndestructibleBox();
        ImGui::TextWrapped("Static box: 2 x 3 x 2 m, origin at its base. Shots cannot destroy it. Edit position/rotation in Map Tree, then Save.");
        if (ImGui::Button("Refresh Models")) RefreshAssets();
        ImGui::SameLine();
        ImGui::Text("glTF / GLB: %zu", m_assets.size());
        if (!m_assetError.empty()) ImGui::TextWrapped("Model search failed: %s", m_assetError.c_str());
        else if (m_assets.empty()) ImGui::TextWrapped("No .gltf or .glb files found in this folder or its subfolders.");
        if (ImGui::BeginListBox("##MapAssets", ImVec2(-1, 130)))
        {
            for (size_t index = 0; index < m_assets.size(); ++index)
            {
                const auto& asset = m_assets[index];
                // Hidden labels in filenames must not become ImGui identifiers.
                ImGui::PushID(static_cast<int>(index));
                const ImVec2 labelPosition = ImGui::GetCursorScreenPos();
                if (ImGui::Selectable("##asset", asset == m_selectedAsset))
                {
                    m_selectedAsset = asset;
                    m_roles = {};
                    m_inspectedAsset.clear();
                    m_roleError.clear();
                    m_materialColors.clear();
                    m_materialColorLoaded = false;
                    m_selectedMaterialIndex = 0;
                    m_materialCopyName = std::filesystem::path(asset).stem().string() + "_Color";
                }
                ImGui::GetWindowDrawList()->AddText(labelPosition, ImGui::GetColorU32(ImGuiCol_Text), asset.c_str());
                ImGui::PopID();
            }
            ImGui::EndListBox();
        }
        if (!m_selectedAsset.empty()) ImGui::TextWrapped("Selected: %s", m_selectedAsset.c_str());
        ImGui::BeginDisabled(m_selectedAsset.empty());
        if (ImGui::Button("Inspect Roles"))
        {
            m_roles = {};
            m_inspectedAsset.clear();
            const std::u8string relativePath(m_selectedAsset.begin(), m_selectedAsset.end());
            if (Tank::Map::InspectGltfRoles(m_map.Folder() / std::filesystem::path(relativePath), m_roles, m_roleError))
                m_inspectedAsset = m_selectedAsset;
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::BeginDisabled(m_selectedAsset != m_inspectedAsset);
        if (ImGui::Button("Add to Map")) AddSelectedModel();
        ImGui::EndDisabled();
        if (!m_roleError.empty()) ImGui::TextWrapped("Role inspection failed: %s", m_roleError.c_str());
        if (!m_inspectedAsset.empty())
        {
            const auto visualCount = std::count_if(m_roles.meshNodes.begin(), m_roles.meshNodes.end(),
                [](const auto& node) { return node.role == Tank::Map::MeshRole::Visual; });
            ImGui::Text("Mesh nodes - Visual: %zu, Hit: %zu", static_cast<size_t>(visualCount),
                m_roles.meshNodes.size() - static_cast<size_t>(visualCount));
            if (visualCount == 0)
            {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.72f, 0.2f, 1.0f));
                ImGui::TextWrapped("Warning: no Visual Mesh. Hit Mesh will be shown as the fallback.");
                ImGui::PopStyleColor();
            }
            for (const auto& node : m_roles.meshNodes)
                ImGui::Text("%s - node %zu, mesh %zu: %s", node.role == Tank::Map::MeshRole::Visual ? "Visual" : "Hit",
                    node.nodeIndex, node.meshIndex, node.name.empty() ? "(unnamed)" : node.name.c_str());
        }
        ImGui::TextWrapped("Visual and Hit display can be toggled above. Hit meshes provide collision during play. Select a Mesh in the Tree to set Breakable and HP.");

        ImGui::SeparatorText("Material Color");
        ImGui::BeginDisabled(m_selectedAsset.empty());
        if (ImGui::Button("Load Materials"))
        {
            const std::u8string pathUtf8(m_selectedAsset.begin(), m_selectedAsset.end());
            m_materialColors.clear();
            m_materialColorLoaded = Tank::Map::InspectGltfMaterialColors(
                m_map.Folder() / std::filesystem::path(pathUtf8), m_materialColors, m_roleError);
            m_selectedMaterialIndex = 0;
            if (m_materialColorLoaded && !m_materialColors.empty())
                m_materialColorEdit = m_materialColors.front().baseColorFactor;
            if (m_materialColorLoaded && m_materialColors.empty())
                m_roleError = "This model has no materials.";
        }
        ImGui::EndDisabled();
        if (m_materialColorLoaded && !m_materialColors.empty())
        {
            std::string materialLabel = m_materialColors[static_cast<size_t>(m_selectedMaterialIndex)].name;
            if (materialLabel.empty()) materialLabel = "Material " + std::to_string(m_selectedMaterialIndex);
            if (ImGui::BeginCombo("Material", materialLabel.c_str()))
            {
                for (size_t index = 0; index < m_materialColors.size(); ++index)
                {
                    const std::string label = m_materialColors[index].name.empty()
                        ? "Material " + std::to_string(index) : m_materialColors[index].name;
                    const bool selected = static_cast<size_t>(m_selectedMaterialIndex) == index;
                    ImGui::PushID(static_cast<int>(index));
                    if (ImGui::Selectable(label.c_str(), selected))
                    {
                        m_selectedMaterialIndex = static_cast<int>(index);
                        m_materialColorEdit = m_materialColors[index].baseColorFactor;
                    }
                    if (selected) ImGui::SetItemDefaultFocus();
                    ImGui::PopID();
                }
                ImGui::EndCombo();
            }
            ImGui::ColorEdit4("Base Color", m_materialColorEdit.data());
            if (ImGui::RadioButton("Override source file", m_materialOverrideFile))
                m_materialOverrideFile = true;
            ImGui::SameLine();
            if (ImGui::RadioButton("Rename copy", !m_materialOverrideFile))
                m_materialOverrideFile = false;
            if (!m_materialOverrideFile)
                ImGui::InputText("Copy file name", &m_materialCopyName);
            if (ImGui::Button("Save Material Color")) SaveSelectedMaterialColor();
        }
        if (!m_roleError.empty()) ImGui::TextWrapped("Material: %s", m_roleError.c_str());

        DrawEnemies();

    }
    if (!m_status.empty()) ImGui::TextWrapped("%s", m_status.c_str());
    ImGui::TextUnformatted("Camera: left drag orbit, middle drag pan, wheel zoom");
    ImGui::TextUnformatted("ESC: Back to Menu");
    ImGui::EndChild();

    if (m_confirm) ImGui::OpenPopup("Unsaved map changes");
    if (ImGui::BeginPopupModal("Unsaved map changes", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::TextUnformatted("Save your changes before continuing?");
        if (ImGui::Button("Save and Continue") && Save())
        {
            m_confirm = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Discard"))
        {
            m_map.DiscardChanges();
            ClearUndoHistory();
            m_sceneReloadRequested = true;
            m_confirm = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel"))
        {
            m_confirm = false;
            m_pending = Action::None;
            ImGui::CloseCurrentPopup();
        }
        if (!m_status.empty()) ImGui::TextWrapped("%s", m_status.c_str());
        ImGui::EndPopup();
    }
    const auto afterView = std::apply([](const auto&... value) { return std::make_tuple(value...); }, viewFields());
    ObserveViewEdit(beforeView, afterView, ImGui::GetActiveID(),
        [this](const auto& value)
        {
            std::tie(m_showGrid, m_gridSnapEnabled, m_gridSpacingMeters, m_gridHalfCellCount,
                m_gridLineWidthMeters, m_showVisualMeshes, m_showHitMeshes, m_visualMeshColor,
                m_gizmoEnabled, m_gizmoLocal, m_gizmoOperation, m_showCameraWindow, m_showCheatSheet) = value;
            m_sceneReloadRequested = true;
        }, "Map preview settings");
    ImGui::End();
    DrawCheatSheet();
    if (m_map.IsOpen() && m_enemyUnitEditor.open)
    {
        const auto before = m_enemyCatalog;
        m_enemyUnitEditor.Draw(m_enemyCatalog, m_enemyCatalogPath);
        if (!(before == m_enemyCatalog))
        {
            m_viewEdit.Flush();
            FlushPendingGizmoEdit();
            FlushPendingManifestEdit();
        }
        m_enemyCatalogEdit.Update(before, m_enemyCatalog, ImGui::GetActiveID(), m_undoHistory,
            [this](const auto& settings) { ApplyEnemyCatalogSnapshot(settings); }, "Enemy catalog edit");
        m_enemyTypes.clear();
        for (const auto& type : m_enemyCatalog.unitTypes) m_enemyTypes.push_back(type.name);
        if (std::find(m_enemyTypes.begin(), m_enemyTypes.end(), m_selectedEnemyType) == m_enemyTypes.end())
            m_selectedEnemyType = m_enemyTypes.empty() ? "" : m_enemyTypes.front();
    }
    else FlushPendingEnemyCatalogEdit();
    return !m_confirm && m_pending != Action::None ? Execute(owner) : false;
}
