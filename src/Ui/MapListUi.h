#pragma once

#include "Map/MapCatalog.h"

#include <imgui.h>
#include <imgui_internal.h>

namespace Ui
{
    inline bool BeginMapListTable(const char* id, float height)
    {
        if (!ImGui::BeginTable(id, 3, ImGuiTableFlags_Resizable | ImGuiTableFlags_RowBg |
            ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_ScrollY, ImVec2(0.0f, height))) return false;
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Directory", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Playable", ImGuiTableColumnFlags_WidthFixed,
            ImGui::CalcTextSize("Playable").x + ImGui::GetStyle().CellPadding.x * 2.0f);
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableHeadersRow();
        return true;
    }

    // Status is read-only here; the open map's Playable checkbox owns the edit history.
    inline bool DrawMapListRow(const Tank::Map::Manifest& document,
        const std::filesystem::path& folder, bool selected)
    {
        const auto pathUtf8 = folder.u8string();
        const std::string id(pathUtf8.begin(), pathUtf8.end());
        ImGui::PushID(id.c_str());
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        const ImVec2 namePosition = ImGui::GetCursorScreenPos();
        const bool clicked = ImGui::Selectable("##map", selected, ImGuiSelectableFlags_SpanAllColumns);
        const auto name = Tank::Map::MapName(document, folder);
        ImGui::GetWindowDrawList()->AddText(namePosition, ImGui::GetColorU32(ImGuiCol_Text), name.c_str());
        ImGui::TableSetColumnIndex(1);
        const auto directoryUtf8 = folder.filename().u8string();
        const std::string directory(directoryUtf8.begin(), directoryUtf8.end());
        ImGui::TextUnformatted(directory.c_str());
        ImGui::TableSetColumnIndex(2);
        const float size = ImGui::GetFontSize();
        const ImVec2 markPosition = ImGui::GetCursorScreenPos();
        ImGui::Dummy(ImVec2(size, size));
        if (document.playable)
            ImGui::RenderCheckMark(ImGui::GetWindowDrawList(), markPosition,
                ImGui::GetColorU32(ImVec4(0.25f, 0.85f, 0.4f, 1.0f)), size);
        ImGui::PopID();
        return clicked;
    }
}
