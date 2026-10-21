#include "TankCollisionProfileEditor.h"

#include <imgui.h>
#include <imgui_stdlib.h>

#include <algorithm>

namespace Ui
{
    namespace
    {
        bool DrawShape(Tank::Physics::TankShapeSettings& shape)
        {
            bool changed = false;
            int kind = static_cast<int>(shape.kind);
            if (ImGui::Combo("Shape", &kind, "Box\0Capsule (+Z axis)\0"))
            {
                shape.kind = static_cast<Tank::Physics::TankShapeKind>(kind);
                if (shape.kind == Tank::Physics::TankShapeKind::Capsule)
                {
                    const float diameter = std::max(shape.sizeMeters.x,
                        shape.sizeMeters.y);
                    shape.sizeMeters.x = diameter;
                    shape.sizeMeters.y = diameter;
                    shape.sizeMeters.z = std::max(shape.sizeMeters.z, diameter);
                }
                changed = true;
            }
            changed |= ImGui::DragFloat3("Position (m)",
                &shape.localPosition.x, 0.02f, -100.0f, 100.0f, "%.2f",
                ImGuiSliderFlags_AlwaysClamp);
            changed |= ImGui::DragFloat3("Rotation (deg)",
                &shape.localRotationDegrees.x, 0.5f, -180.0f, 180.0f,
                "%.1f", ImGuiSliderFlags_AlwaysClamp);
            if (shape.kind == Tank::Physics::TankShapeKind::Capsule)
            {
                float diameter = shape.sizeMeters.x;
                if (ImGui::DragFloat("Diameter (m)", &diameter, 0.02f,
                        0.05f, 100.0f, "%.2f",
                        ImGuiSliderFlags_AlwaysClamp))
                {
                    shape.sizeMeters.x = diameter;
                    shape.sizeMeters.y = diameter;
                    shape.sizeMeters.z = std::max(shape.sizeMeters.z, diameter);
                    changed = true;
                }
                changed |= ImGui::DragFloat("Total length (m)",
                    &shape.sizeMeters.z, 0.02f, diameter, 100.0f, "%.2f",
                    ImGuiSliderFlags_AlwaysClamp);
            }
            else
            {
                changed |= ImGui::DragFloat3("Scale / full size (m)",
                    &shape.sizeMeters.x, 0.02f, 0.05f, 100.0f, "%.2f",
                    ImGuiSliderFlags_AlwaysClamp);
            }
            return changed;
        }
    }

    bool DrawTankCollisionProfileEditor(const char* id,
        Tank::Physics::TankCollisionProfile& profile)
    {
        bool changed = false;
        ImGui::PushID(id);
        if (ImGui::TreeNode("Movement Shape"))
        {
            ImGui::TextWrapped("Used only for movement and Map collision.");
            changed |= DrawShape(profile.movementShape);
            ImGui::TreePop();
        }
        if (ImGui::TreeNode("Hit Regions"))
        {
            ImGui::TextWrapped("Projectile hits and impact effects use these child regions, not the movement shape.");
            if (profile.hitRegions.size() < 16 && ImGui::Button("Add Hit Region"))
            {
                Tank::Physics::TankHitRegion region;
                region.name = "Region " +
                    std::to_string(profile.hitRegions.size() + 1);
                profile.hitRegions.push_back(std::move(region));
                changed = true;
            }
            for (size_t index = 0; index < profile.hitRegions.size();)
            {
                ImGui::PushID(static_cast<int>(index));
                auto& region = profile.hitRegions[index];
                bool remove = false;
                if (ImGui::TreeNode("Region", "%zu: %s", index,
                        region.name.c_str()))
                {
                    changed |= ImGui::InputText("Name", &region.name);
                    changed |= DrawShape(region.shape);
                    ImGui::BeginDisabled(profile.hitRegions.size() == 1);
                    remove = ImGui::Button("Remove Hit Region");
                    ImGui::EndDisabled();
                    ImGui::TreePop();
                }
                ImGui::PopID();
                if (remove)
                {
                    profile.hitRegions.erase(profile.hitRegions.begin() + index);
                    changed = true;
                }
                else
                {
                    ++index;
                }
            }
            ImGui::TreePop();
        }
        if (!Tank::Physics::IsValidTankCollisionProfile(profile))
        {
            ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.2f, 1.0f),
                "Collision profile is invalid.");
        }
        ImGui::PopID();
        return changed;
    }
}
