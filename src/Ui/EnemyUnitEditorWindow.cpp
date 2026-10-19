#include "EnemyUnitEditorWindow.h"

#include "Physics/EnemyEditorJson.h"
#include "Physics/TrackedVehicleTest.h"
#include <imgui.h>
#include <ImGuiWidgets.h>
#include <algorithm>
#include <imgui_stdlib.h>
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <cstdio>

namespace Ui
{
    void EnemyUnitEditorWindow::Draw(Tank::Physics::EnemyEditorSettings& editor, std::string& jsonPath,
        const Tank::Physics::TrackedVehicleTestState* state, const std::function<void()>& applyAndReset)
    {
        if (!open) return;
        ImGui::SetNextWindowSize(ImVec2(620, 700), ImGuiCond_FirstUseEver);
        if (!ImGui::Begin("Enemy Unit Editor", &open)) {ImGui::End(); return;}
        if (applyAndReset && ImGui::Button("Apply & Reset"))
        {
            std::string validated;
            if (Tank::Physics::SerializeEnemyEditor(editor, validated, m_status)) applyAndReset();
        }


        ImGui::TextUnformatted("Enemy movement simulation (applies during play)");
        ImGui::DragFloat("Enter tracked physics (m)", &editor.simulation.physicalEnterMeters,
            1, 1, editor.simulation.simpleReturnMeters - 1, "%.0f", ImGuiSliderFlags_AlwaysClamp);
        ImGui::DragFloat("Return to simple movement (m)", &editor.simulation.simpleReturnMeters,
            1, editor.simulation.physicalEnterMeters + 1, 10000, "%.0f", ImGuiSliderFlags_AlwaysClamp);
        ImGui::DragFloat("Enemy braking deceleration (m/s2)", &editor.simulation.brakingDecelerationMetersPerSecondSquared,
            0.1f, 0.1f, 20, "%.1f", ImGuiSliderFlags_AlwaysClamp);
        ImGui::DragFloat("Enemy route node arrival (m)", &editor.simulation.nodeArrivalMeters,
            0.05f, 0.1f, 2, "%.2f", ImGuiSliderFlags_AlwaysClamp);
        ImGui::DragFloat("Observed target node radius (m)", &editor.simulation.observationRadiusMeters,
            0.1f, 0.1f, 100, "%.1f", ImGuiSliderFlags_AlwaysClamp);
        ImGui::DragFloat("Observed support height tolerance (m)", &editor.simulation.supportHeightToleranceMeters,
            0.01f, 0.01f, 2, "%.2f", ImGuiSliderFlags_AlwaysClamp);
        ImGui::DragFloat("Simple pivot speed (deg/s)", &editor.simulation.simpleTurnDegreesPerSecond,
            1, 1, 360, "%.0f", ImGuiSliderFlags_AlwaysClamp);
        if (state && ImGui::TreeNode("Enemy movement status"))
        {
            if (ImGui::BeginTable("PlacedEnemyMovement", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
            {
                ImGui::TableSetupColumn("Enemy"); ImGui::TableSetupColumn("Mode");
                ImGui::TableSetupColumn("Distance (m)"); ImGui::TableSetupColumn("HP"); ImGui::TableHeadersRow();
                for (const auto& enemy : state->fixedTurrets)
                {
                    ImGui::TableNextRow(); ImGui::TableNextColumn(); ImGui::TextUnformatted(enemy.placementId.c_str());
                    ImGui::TableNextColumn(); ImGui::TextUnformatted(!enemy.target.active ? "Destroyed" :
                        (enemy.physicalMoving ? "Tracked physics" : (enemy.simpleMoving ? "Simple" : "Fixed")));
                    ImGui::TableNextColumn(); ImGui::Text("%.1f", Tank::Physics::NavigationDistance(enemy.position, state->bodyPosition));
                    ImGui::TableNextColumn(); ImGui::Text("%.0f", enemy.target.hitPoints);
                }
                ImGui::EndTable();
            }
            constexpr const char* actions[] = {"Hold", "Patrol", "Approach", "Attack stop", "Search", "Search wait"};
            for (const auto& group : state->enemyGroups)
            {
                ImGui::Text("%s: %s  Target surface: %s", group.id.c_str(),
                    group.behavior ? actions[static_cast<size_t>(group.behavior->action)] : "None",
                    group.behavior ? group.behavior->targetSurfaceId.c_str() : "");
                if (group.navigation && !group.navigation->error.empty()) ImGui::TextWrapped("Route: %s", group.navigation->error.c_str());
            }
            ImGui::TreePop();
        }
        auto& jsonStatus = m_status;
        ImGui::InputText("Enemy types JSON path", &jsonPath);
        if (ImGui::Button("Save enemy types JSON"))
        {
            std::string json;
            if (Tank::Physics::SerializeEnemyEditor(editor, json, jsonStatus))
            {
                const auto path = std::filesystem::path(std::u8string(jsonPath.begin(), jsonPath.end()));
                auto temporary = path;
                temporary += L".tmp";
                std::ofstream file(temporary, std::ios::binary);
                if (file)
                {
                    // Store project JSON with CRLF, regardless of ostream text mode.
                    std::string crlf;
                    for (const char character : json) { if (character == '\n') crlf += '\r'; crlf += character; }
                    file << crlf << "\r\n";
                    file.close();
                    if (file && MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
                        jsonStatus = "Saved: " + jsonPath;
                    else jsonStatus = "Failed to save; original file retained: " + jsonPath;
                }
                else jsonStatus = "Cannot open for writing: " + jsonPath;
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Load enemy types JSON"))
        {
            std::ifstream file(std::filesystem::path(std::u8string(jsonPath.begin(), jsonPath.end())), std::ios::binary);
            if (!file) jsonStatus = "Cannot open for reading: " + jsonPath;
            else
            {
                const std::string json((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
                if (file.bad()) jsonStatus = "Failed to read: " + jsonPath;
                else if (Tank::Physics::DeserializeEnemyEditor(json, editor, jsonStatus)) jsonStatus = "Loaded: " + jsonPath;
            }
        }
        if (!jsonStatus.empty()) ImGui::TextWrapped("%s", jsonStatus.c_str());
        ImGui::TextWrapped("Load replaces the current catalog. Save edits before loading another file. During play, movement settings apply live; Apply & Reset applies attack/template edits.");
        if (ImGui::Button("Register attack type"))
        {
            Tank::Physics::EnemyAttackType type;
            type.name = "Attack " + std::to_string(editor.attackTypes.size());
            editor.attackTypes.push_back(type);
        }
        for (size_t i = 0; i < editor.attackTypes.size(); ++i)
        {
            ImGui::PushID(static_cast<int>(i));
            auto& type = editor.attackTypes[i];
            if (ImGui::TreeNode("Attack", "Attack %zu: %s", i, type.name.c_str()))
            {
                char name[128];
                snprintf(name, sizeof(name), "%s", type.name.c_str());
                if (ImGui::InputText("Name", name, sizeof(name))) type.name = name;
                ImGui::DragFloat("a Detection (m)", &type.detectionRangeMeters, 0.5f, type.reachMeters, 10000, "%.1f", ImGuiSliderFlags_AlwaysClamp);
                ImGui::DragFloat("b Reach (m)", &type.reachMeters, 0.5f, type.firingRangeMeters, type.detectionRangeMeters, "%.1f", ImGuiSliderFlags_AlwaysClamp);
                ImGui::DragFloat("c Firing range (m)", &type.firingRangeMeters, 0.5f, 0.1f, type.reachMeters, "%.1f", ImGuiSliderFlags_AlwaysClamp);
                ImGui::DragFloat("d Bullet speed (m/s)", &type.projectileSpeedMetersPerSecond, 0.1f, 0.1f, 10000, "%.1f", ImGuiSliderFlags_AlwaysClamp);
                ImGui::DragFloat("e Fire interval (s)", &type.firingIntervalSeconds, 0.1f, 0.01f, 3600, "%.2f", ImGuiSliderFlags_AlwaysClamp);
                ImGui::DragFloat("f Max turret yaw (deg/s)", &type.maximumYawSpeedDegreesPerSecond, 0.5f, 0.1f, 3600, "%.1f", ImGuiSliderFlags_AlwaysClamp);
                ImGui::DragFloat("g Aim tolerance (+/- deg)", &type.firingToleranceDegrees, 0.1f, 0, 180, "%.1f", ImGuiSliderFlags_AlwaysClamp);
                int detectionMode = static_cast<int>(type.detectionMode);
                if (ImGui::Combo("Detection mode", &detectionMode, "Optical (ray occlusion)\0Range only\0"))
                    type.detectionMode = static_cast<Tank::Physics::EnemyDetectionMode>(detectionMode);
                ImGui::DragFloat("Alert release delay (s)", &type.alertReleaseSeconds, 0.1f, 0, 3600, "%.1f", ImGuiSliderFlags_AlwaysClamp);
                ImGui::TextWrapped("Lost target: aim at last known position without firing. After delay: return to initial direction. During play, Apply & Reset applies changes.");
                int projectileKind = static_cast<int>(type.projectileKind);
                if (ImGui::Combo("Projectile kind", &projectileKind, "Ordinary (interceptable)\0Special (unbreakable)\0"))
                {
                    type.projectileKind = static_cast<Tank::Physics::EnemyProjectileKind>(projectileKind);
                    if (type.projectileKind == Tank::Physics::EnemyProjectileKind::Ordinary)
                        type.projectileShape = Tank::Physics::EnemyProjectileShape::Sphere;
                }
                if (type.projectileKind == Tank::Physics::EnemyProjectileKind::Special)
                {
                    int shape = static_cast<int>(type.projectileShape);
                    if (ImGui::Combo("Bullet shape", &shape, "Sphere\0Box\0"))
                        type.projectileShape = static_cast<Tank::Physics::EnemyProjectileShape>(shape);
                }
                if (type.projectileShape == Tank::Physics::EnemyProjectileShape::Box)
                {
                    float size[3] = {type.projectileBoxSizeMeters.x, type.projectileBoxSizeMeters.y, type.projectileBoxSizeMeters.z};
                    if (ImGui::DragFloat3("Width / Height / Depth (m)", size, 0.05f, 0.1f, 20, "%.2f", ImGuiSliderFlags_AlwaysClamp))
                        type.projectileBoxSizeMeters = {size[0], size[1], size[2]};
                    ImGui::TextDisabled("Depth follows the firing direction. During play, changes apply with Apply & Reset.");
                }
                else ImGui::DragFloat("Bullet radius (m)", &type.projectileRadiusMeters, 0.05f, 0.05f, 10, "%.2f", ImGuiSliderFlags_AlwaysClamp);
                if (ImGui::Button("Restore default parameters"))
                {
                    const auto savedName = type.name;
                    type = {};
                    type.name = savedName;
                }
                if (!Tank::Physics::IsValidEnemyAttackType(type)) ImGui::TextUnformatted("Invalid attack parameters");
                ImGui::TreePop();
            }
            ImGui::PopID();
        }
        if (ImGui::Button("Register enemy unit type"))
        {
            Tank::Physics::EnemyUnitType unit;
            unit.name = "Enemy " + std::to_string(editor.unitTypes.size());
            editor.unitTypes.push_back(unit);
        }
        for (size_t i = 0; i < editor.unitTypes.size(); ++i)
        {
            ImGui::PushID(static_cast<int>(i) + 100000);
            auto& unit = editor.unitTypes[i];
            if (ImGui::TreeNode("Unit", "Enemy %zu: %s", i, unit.name.c_str()))
            {
                char name[128];
                snprintf(name, sizeof(name), "%s", unit.name.c_str());
                if (ImGui::InputText("Name", name, sizeof(name))) unit.name = name;
                ImGuiWidgets::SliderFloatWithControls("HP", &unit.hitPoints, 1, 100000, 10, 60, "%.0f");
                unit.hitPoints = std::clamp(unit.hitPoints, 1.0f, 100000.0f);
                if (ImGui::Button("Add attack mount")) unit.attackMounts.push_back({});
                for (size_t mountIndex = 0; mountIndex < unit.attackMounts.size();)
                {
                    ImGui::PushID(static_cast<int>(mountIndex));
                    auto& mount = unit.attackMounts[mountIndex];
                    ImGui::Text("Mount %zu", mountIndex);
                    ImGui::DragFloat3("Local position (m)", &mount.localPosition.x, 0.1f, -1000, 1000, "%.2f", ImGuiSliderFlags_AlwaysClamp);
                    const auto& selected = editor.attackTypes[static_cast<size_t>(mount.attackTypeIndex)];
                    if (ImGui::BeginCombo("Attack type", selected.name.c_str()))
                    {
                        for (size_t attack = 0; attack < editor.attackTypes.size(); ++attack)
                        {
                            ImGui::PushID(static_cast<int>(attack));
                            if (ImGui::Selectable(editor.attackTypes[attack].name.c_str(), mount.attackTypeIndex == static_cast<int>(attack)))
                                mount.attackTypeIndex = static_cast<int>(attack);
                            ImGui::PopID();
                        }
                        ImGui::EndCombo();
                    }
                    const bool remove = ImGui::Button("Remove mount");
                    ImGui::PopID();
                    if (remove) unit.attackMounts.erase(unit.attackMounts.begin() + mountIndex);
                    else ++mountIndex;
                }
                ImGui::TreePop();
            }
            ImGui::PopID();
        }

        if (!applyAndReset)
            ImGui::TextWrapped("Save the catalog before starting play. The map uses this catalog's unit names.");
        ImGui::End();
    }
}