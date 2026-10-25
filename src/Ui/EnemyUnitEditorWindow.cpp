#include "EnemyUnitEditorWindow.h"
#include "TankCollisionProfileEditor.h"

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
    namespace
    {
        bool EnemySlider(const char* label, float* value, float minimum, float maximum,
            float step, float defaultValue, const char* format)
        {
            maximum = (std::max)(minimum, maximum);
            return ImGuiWidgets::SliderFloatWithControls(label, value, minimum, maximum, step,
                std::clamp(defaultValue, minimum, maximum), format, ImGuiSliderFlags_AlwaysClamp);
        }
    }

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
        ImGui::Checkbox("Enemy movement enabled", &editor.simulation.movementEnabled);
        if (editor.simulation.movementEnabled)
        {
            const Tank::Physics::EnemySimulationSettings defaults;
            EnemySlider("Enter tracked physics (m)", &editor.simulation.physicalEnterMeters,
                1, editor.simulation.simpleReturnMeters - 1, 1, defaults.physicalEnterMeters, "%.0f");
            EnemySlider("Return to simple movement (m)", &editor.simulation.simpleReturnMeters,
                editor.simulation.physicalEnterMeters + 1, 10000, 1, defaults.simpleReturnMeters, "%.0f");
            EnemySlider("Enemy braking deceleration (m/s2)", &editor.simulation.brakingDecelerationMetersPerSecondSquared,
                0.1f, 20, 0.1f, defaults.brakingDecelerationMetersPerSecondSquared, "%.1f");
            EnemySlider("Enemy route node arrival (m)", &editor.simulation.nodeArrivalMeters,
                0.1f, 2, 0.05f, defaults.nodeArrivalMeters, "%.2f");
            EnemySlider("Observed target node radius (m)", &editor.simulation.observationRadiusMeters,
                0.1f, 100, 0.1f, defaults.observationRadiusMeters, "%.1f");
            EnemySlider("Observed support height tolerance (m)", &editor.simulation.supportHeightToleranceMeters,
                0.01f, 2, 0.01f, defaults.supportHeightToleranceMeters, "%.2f");
            EnemySlider("Simple pivot speed (deg/s)", &editor.simulation.simpleTurnDegreesPerSecond,
                1, 360, 1, defaults.simpleTurnDegreesPerSecond, "%.0f");
        }
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
            if (editor.simulation.movementEnabled)
            {
                for (const auto& group : state->enemyGroups)
                {
                    ImGui::Text("%s: %s  Target surface: %s", group.id.c_str(),
                        group.behavior ? actions[static_cast<size_t>(group.behavior->action)] : "None",
                        group.behavior ? group.behavior->targetSurfaceId.c_str() : "");
                    if (group.navigation && !group.navigation->error.empty()) ImGui::TextWrapped("Route: %s", group.navigation->error.c_str());
                }
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
            const Tank::Physics::EnemyAttackType defaults;
            if (ImGui::TreeNode("Attack", "Attack %zu: %s", i, type.name.c_str()))
            {
                char name[128];
                snprintf(name, sizeof(name), "%s", type.name.c_str());
                if (ImGui::InputText("Name", name, sizeof(name))) type.name = name;
                EnemySlider("a Detection (m)", &type.detectionRangeMeters, type.reachMeters, 10000, 0.5f, defaults.detectionRangeMeters, "%.1f");
                EnemySlider("b Reach (m)", &type.reachMeters, type.firingRangeMeters, type.detectionRangeMeters, 0.5f, defaults.reachMeters, "%.1f");
                EnemySlider("c Firing range (m)", &type.firingRangeMeters, 0.1f, type.reachMeters, 0.5f, defaults.firingRangeMeters, "%.1f");
                EnemySlider("d Bullet speed (m/s)", &type.projectileSpeedMetersPerSecond, 0.1f, 10000, 0.1f, defaults.projectileSpeedMetersPerSecond, "%.1f");
                EnemySlider("e Fire interval (s)", &type.firingIntervalSeconds, 0.01f, 3600, 0.1f, defaults.firingIntervalSeconds, "%.2f");
                EnemySlider("f Max turret yaw (deg/s)", &type.maximumYawSpeedDegreesPerSecond, 0.1f, 3600, 0.5f, defaults.maximumYawSpeedDegreesPerSecond, "%.1f");
                EnemySlider("g Aim tolerance (+/- deg)", &type.firingToleranceDegrees, 0, 180, 0.1f, defaults.firingToleranceDegrees, "%.1f");
                int detectionMode = static_cast<int>(type.detectionMode);
                if (ImGui::Combo("Detection mode", &detectionMode, "Optical (ray occlusion)\0Range only\0"))
                    type.detectionMode = static_cast<Tank::Physics::EnemyDetectionMode>(detectionMode);
                EnemySlider("Alert release delay (s)", &type.alertReleaseSeconds, 0, 3600, 0.1f, defaults.alertReleaseSeconds, "%.1f");
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
                    EnemySlider("Bullet width (m)", &type.projectileBoxSizeMeters.x, 0.1f, 20, 0.05f,
                        defaults.projectileBoxSizeMeters.x, "%.2f");
                    EnemySlider("Bullet height (m)", &type.projectileBoxSizeMeters.y, 0.1f, 20, 0.05f,
                        defaults.projectileBoxSizeMeters.y, "%.2f");
                    EnemySlider("Bullet depth (m)", &type.projectileBoxSizeMeters.z, 0.1f, 20, 0.05f,
                        defaults.projectileBoxSizeMeters.z, "%.2f");
                    ImGui::TextDisabled("Depth follows the firing direction. During play, changes apply with Apply & Reset.");
                }
                else EnemySlider("Bullet radius (m)", &type.projectileRadiusMeters, 0.05f, 10, 0.05f, defaults.projectileRadiusMeters, "%.2f");
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
                DrawTankCollisionProfileEditor("UnitCollision",
                    unit.collisionProfile);
                if (ImGui::Button("Add attack mount")) unit.attackMounts.push_back({});
                for (size_t mountIndex = 0; mountIndex < unit.attackMounts.size();)
                {
                    ImGui::PushID(static_cast<int>(mountIndex));
                    auto& mount = unit.attackMounts[mountIndex];
                    ImGui::Text("Mount %zu", mountIndex);
                    EnemySlider("Mount X (m)", &mount.localPosition.x, -1000, 1000, 0.1f, 0, "%.2f");
                    EnemySlider("Mount Y (m)", &mount.localPosition.y, -1000, 1000, 0.1f, 0, "%.2f");
                    EnemySlider("Mount Z (m)", &mount.localPosition.z, -1000, 1000, 0.1f, 0, "%.2f");
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
