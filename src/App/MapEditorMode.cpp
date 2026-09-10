#include "MapEditorMode.h"

#include "Map/MapAssetCatalog.h"
#include "Map/MapEditing.h"
#include "Platform/Windows/MapFolderPicker.h"
#include <algorithm>
#include <imgui.h>
#include <imgui_stdlib.h>

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
    Tank::Map::Manifest updated = m_map.Document();
    Tank::Map::Instance instance;
    instance.id = Tank::Map::MakeUniqueInstanceId(updated);
    instance.asset = m_selectedAsset;
    updated.instances.push_back(std::move(instance));
    std::string error;
    if (!m_map.SetManifest(updated, error))
    {
        m_status = "Could not add model: " + error;
        return false;
    }
    m_sceneReloadRequested = true;
    m_selectedInstanceId = updated.instances.back().id;
    m_status = "Added " + m_selectedAsset + ". Save to write Manifest.json.";
    return true;
}

bool MapEditorMode::UpdateSelectedInstance(const Tank::Map::Transform& transform)
{
    Tank::Map::Manifest updated = m_map.Document();
    const auto instance = std::find_if(updated.instances.begin(), updated.instances.end(),
        [this](const Tank::Map::Instance& value) { return value.id == m_selectedInstanceId; });
    if (instance == updated.instances.end())
    {
        m_status = "The selected model no longer exists.";
        m_selectedInstanceId.clear();
        return false;
    }
    instance->transform = transform;
    std::string error;
    if (!m_map.SetManifest(updated, error))
    {
        m_status = "Could not update transform: " + error;
        return false;
    }
    m_sceneReloadRequested = true;
    return true;
}

bool MapEditorMode::RemoveSelectedInstance()
{
    Tank::Map::Manifest updated = m_map.Document();
    const auto remove = std::find_if(updated.instances.begin(), updated.instances.end(),
        [this](const Tank::Map::Instance& value) { return value.id == m_selectedInstanceId; });
    if (remove == updated.instances.end()) return false;
    const std::string removedAsset = remove->asset;
    Tank::Map::RemoveInstance(updated, m_selectedInstanceId);
    std::string error;
    if (!m_map.SetManifest(updated, error))
    {
        m_status = "Could not remove model: " + error;
        return false;
    }
    m_selectedInstanceId.clear();
    m_sceneReloadRequested = true;
    m_status = "Removed " + removedAsset + ". Save to write Manifest.json.";
    return true;
}

bool MapEditorMode::DuplicateSelectedInstance()
{
    Tank::Map::Manifest updated = m_map.Document();
    const std::optional<std::string> duplicateId =
        Tank::Map::DuplicateInstance(updated, m_selectedInstanceId);
    if (!duplicateId) return false;
    std::string error;
    if (!m_map.SetManifest(updated, error))
    {
        m_status = "Could not duplicate model: " + error;
        return false;
    }
    m_selectedInstanceId = *duplicateId;
    m_sceneReloadRequested = true;
    m_status = "Duplicated model. Move it, then Save to write Manifest.json.";
    return true;
}

bool MapEditorMode::UpdatePlayerSpawn(const Tank::Map::Transform& transform)
{
    Tank::Map::Manifest updated = m_map.Document();
    updated.playerSpawn = transform;
    std::string error;
    if (!m_map.SetManifest(updated, error))
    {
        m_status = "Could not update player start: " + error;
        return false;
    }
    m_sceneReloadRequested = true;
    return true;
}

bool MapEditorMode::AddClearArea()
{
    Tank::Map::Manifest updated = m_map.Document();
    const std::string id = Tank::Map::AddClearArea(updated);
    std::string error;
    if (!m_map.SetManifest(updated, error))
    {
        m_status = "Could not add clear area: " + error;
        return false;
    }
    m_selectedClearAreaId = id;
    m_sceneReloadRequested = true;
    m_status = "Added clear area. Save to write Manifest.json.";
    return true;
}

bool MapEditorMode::UpdateSelectedClearArea(const Tank::Map::ClearArea& area)
{
    Tank::Map::Manifest updated = m_map.Document();
    const auto selected = std::find_if(updated.clearAreas.begin(), updated.clearAreas.end(),
        [this](const Tank::Map::ClearArea& value) { return value.id == m_selectedClearAreaId; });
    if (selected == updated.clearAreas.end())
    {
        m_status = "The selected clear area no longer exists.";
        m_selectedClearAreaId.clear();
        return false;
    }
    *selected = area;
    std::string error;
    if (!m_map.SetManifest(updated, error))
    {
        m_status = "Could not update clear area: " + error;
        return false;
    }
    m_sceneReloadRequested = true;
    return true;
}

bool MapEditorMode::RemoveSelectedClearArea()
{
    Tank::Map::Manifest updated = m_map.Document();
    if (!Tank::Map::RemoveClearArea(updated, m_selectedClearAreaId)) return false;
    std::string error;
    if (!m_map.SetManifest(updated, error))
    {
        m_status = "Could not remove clear area: " + error;
        return false;
    }
    m_selectedClearAreaId.clear();
    m_sceneReloadRequested = true;
    m_status = "Removed clear area. Save to write Manifest.json.";
    return true;
}

bool MapEditorMode::Save()
{
    std::string error;
    if (!m_map.Save(error))
    {
        m_status = "Save failed: " + error;
        return false;
    }
    m_status = "Saved Manifest.json";
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
        ImGui::BulletText("Left drag: Orbit");
        ImGui::BulletText("Middle drag: Pan");
        ImGui::BulletText("Mouse wheel: Zoom");
        ImGui::TextWrapped("Move the pointer outside GUI windows before operating the camera.");

        ImGui::SeparatorText("Add a Model");
        ImGui::BulletText("Open a map folder");
        ImGui::BulletText("Select a glTF or GLB file");
        ImGui::BulletText("Inspect Roles");
        ImGui::BulletText("Add to Map");

        ImGui::SeparatorText("Edit the Map");
        ImGui::BulletText("Placed Models: position and rotation");
        ImGui::BulletText("Player Start: chassis-center position and rotation");
        ImGui::TextWrapped("Place Player Start Y above the HitMesh so the tank does not spawn inside the ground.");
        ImGui::BulletText("Clear Areas: goal AABB center and size");
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
        if (m_map.IsOpen()) m_closedMapFolder = m_map.Folder();
        m_map = {};
        m_status.clear();
        m_assets.clear();
        m_selectedAsset.clear();
        m_assetError.clear();
        m_roles = {};
        m_inspectedAsset.clear();
        m_roleError.clear();
        m_selectedInstanceId.clear();
        m_selectedClearAreaId.clear();
        m_sceneReloadRequested = true;
        return true;
    }
    if (action == Action::Open)
    {
        std::filesystem::path selected;
        std::string error;
        const auto result = Tank::Platform::Windows::PickMapFolder(owner, m_map.Folder(), selected, error);
        if (result == Tank::Platform::Windows::FolderPickerResult::Selected)
        {
            if (m_map.Open(selected, error))
            {
                m_status = "Opened Manifest.json";
                m_selectedAsset.clear();
                m_selectedInstanceId.clear();
                m_selectedClearAreaId.clear();
                RefreshAssets();
                m_sceneReloadRequested = true;
            }
            else m_status = "Open failed: " + error;
        }
        else if (result == Tank::Platform::Windows::FolderPickerResult::Failed)
            m_status = error;
    }
    return false;
}

bool MapEditorMode::DrawUi(HWND__* owner)
{
    const auto* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + 10, viewport->WorkPos.y + 10), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(520, 700), ImGuiCond_FirstUseEver);
    ImGui::Begin("Map Editor", nullptr, ImGuiWindowFlags_NoCollapse);
    if (ImGui::Button("Open")) Request(Action::Open);
    ImGui::SameLine();
    ImGui::BeginDisabled(!m_map.IsOpen());
    if (ImGui::Button("Save")) Save();
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button("Back to Menu")) RequestExit();
    ImGui::SameLine();
    if (ImGui::Button("Cheat Sheet")) m_showCheatSheet = true;
    ImGui::Separator();
    if (!m_map.IsOpen())
    {
        ImGui::TextWrapped("Open a map folder to begin. A missing Manifest.json will be created automatically.");
    }
    else
    {
        const auto utf8 = m_map.Folder().u8string();
        const std::string folder(utf8.begin(), utf8.end());
        ImGui::TextWrapped("Folder: %s", folder.c_str());
        ImGui::TextUnformatted(m_map.IsDirty() ? "Unsaved changes" : "Saved");
        const auto& document = m_map.Document();
        ImGui::Text("Models: %zu", document.instances.size());
        ImGui::Text("Clear areas: %zu", document.clearAreas.size());
        ImGui::Text("Player start: %.2f, %.2f, %.2f m", document.playerSpawn.position[0],
            document.playerSpawn.position[1], document.playerSpawn.position[2]);
        ImGui::SeparatorText("Grid");
        bool gridChanged = ImGui::DragFloat("Spacing (m)", &m_gridSpacingMeters, 0.05f, 0.1f, 100.0f, "%.2f");
        gridChanged |= ImGui::SliderInt("Half cells", &m_gridHalfCellCount, 1, 100);
        gridChanged |= ImGui::DragFloat("Line width (m)", &m_gridLineWidthMeters, 0.005f, 0.005f, 0.2f, "%.3f");
        if (gridChanged) m_sceneReloadRequested = true;
        ImGui::Separator();
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
            for (const auto& node : m_roles.meshNodes)
                ImGui::Text("%s - node %zu, mesh %zu: %s", node.role == Tank::Map::MeshRole::Visual ? "Visual" : "Hit",
                    node.nodeIndex, node.meshIndex, node.name.empty() ? "(unnamed)" : node.name.c_str());
        }
        ImGui::TextWrapped("Add to Map loads Visual meshes into the preview. Hit meshes are reserved for physics.");

        ImGui::SeparatorText("Placed Models");
        if (document.instances.empty())
        {
            ImGui::TextUnformatted("No models have been added to this map.");
        }
        else
        {
            if (ImGui::BeginListBox("##PlacedModels", ImVec2(-1, 92)))
            {
                for (size_t index = 0; index < document.instances.size(); ++index)
                {
                    const Tank::Map::Instance& instance = document.instances[index];
                    ImGui::PushID(static_cast<int>(index));
                    const std::string label = instance.id + "  (" + instance.asset + ")";
                    if (ImGui::Selectable(label.c_str(), instance.id == m_selectedInstanceId))
                        m_selectedInstanceId = instance.id;
                    ImGui::PopID();
                }
                ImGui::EndListBox();
            }
            const auto selected = std::find_if(document.instances.begin(), document.instances.end(),
                [this](const Tank::Map::Instance& instance) { return instance.id == m_selectedInstanceId; });
            if (selected != document.instances.end())
            {
                Tank::Map::Transform transform = selected->transform;
                ImGui::TextWrapped("Selected: %s", selected->asset.c_str());
                if (ImGui::Button("Remove Selected"))
                {
                    RemoveSelectedInstance();
                }
                ImGui::SameLine();
                if (ImGui::Button("Duplicate"))
                {
                    DuplicateSelectedInstance();
                }
                const bool positionChanged = ImGui::DragFloat3("Position (m)", transform.position.data(), 0.05f);
                const bool rotationChanged = ImGui::DragFloat3("Rotation (deg)", transform.rotationDegrees.data(), 1.0f);
                if ((positionChanged || rotationChanged) && !UpdateSelectedInstance(transform))
                    m_status = "Transform preview was not updated.";
            }
        }

        ImGui::SeparatorText("Player Start");
        ImGui::TextWrapped("Position is the tank chassis center. Keep Y above the HitMesh.");
        Tank::Map::Transform playerSpawn = document.playerSpawn;
        const bool spawnPositionChanged =
            ImGui::DragFloat3("Position (m)##PlayerStart", playerSpawn.position.data(), 0.05f);
        const bool spawnRotationChanged =
            ImGui::DragFloat3("Rotation (deg)##PlayerStart", playerSpawn.rotationDegrees.data(), 1.0f);
        if ((spawnPositionChanged || spawnRotationChanged) && !UpdatePlayerSpawn(playerSpawn))
            m_status = "Player start preview was not updated.";

        ImGui::SeparatorText("Clear Areas");
        if (ImGui::Button("Add Clear Area")) AddClearArea();
        if (document.clearAreas.empty())
        {
            ImGui::TextUnformatted("No clear areas have been added to this map.");
        }
        else
        {
            if (ImGui::BeginListBox("##ClearAreas", ImVec2(-1, 80)))
            {
                for (size_t index = 0; index < document.clearAreas.size(); ++index)
                {
                    const Tank::Map::ClearArea& area = document.clearAreas[index];
                    ImGui::PushID(static_cast<int>(index));
                    const std::string label = area.id + "  (" + area.name + ")";
                    if (ImGui::Selectable(label.c_str(), area.id == m_selectedClearAreaId))
                        m_selectedClearAreaId = area.id;
                    ImGui::PopID();
                }
                ImGui::EndListBox();
            }
            const auto selected = std::find_if(document.clearAreas.begin(), document.clearAreas.end(),
                [this](const Tank::Map::ClearArea& area) { return area.id == m_selectedClearAreaId; });
            if (selected != document.clearAreas.end())
            {
                Tank::Map::ClearArea area = *selected;
                bool areaChanged = ImGui::InputText("Name##ClearArea", &area.name);
                areaChanged |= ImGui::DragFloat3(
                    "Center (m)##ClearArea", area.center.data(), 0.05f);
                areaChanged |= ImGui::DragFloat3(
                    "Size (m)##ClearArea", area.size.data(), 0.05f, 0.01f, 10000.0f);
                if (areaChanged && !UpdateSelectedClearArea(area))
                    m_status = "Clear area preview was not updated.";
                if (ImGui::Button("Remove Clear Area")) RemoveSelectedClearArea();
            }
        }
    }
    if (!m_status.empty()) ImGui::TextWrapped("%s", m_status.c_str());
    ImGui::TextUnformatted("Camera: left drag orbit, middle drag pan, wheel zoom");
    ImGui::TextUnformatted("ESC: Back to Menu");

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
    ImGui::End();
    DrawCheatSheet();
    return !m_confirm && m_pending != Action::None ? Execute(owner) : false;
}
