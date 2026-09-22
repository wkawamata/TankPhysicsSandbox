#include "stdafx.h"
#include "Ui/OutputPanel.h"
#include "imgui.h"

#include <cstdio>
#include <ctime>

namespace Ui
{
    using namespace Tank::Diagnostics;
    namespace
    {
        ImVec4 LevelColor(LogLevel level)
        {
            switch (level)
            {
            case LogLevel::Trace: return {0.60f, 0.60f, 0.60f, 1.0f};
            case LogLevel::Debug: return {0.45f, 0.80f, 1.00f, 1.0f};
            case LogLevel::Warning: return {1.00f, 0.80f, 0.25f, 1.0f};
            case LogLevel::Error: return {1.00f, 0.40f, 0.40f, 1.0f};
            case LogLevel::Fatal: return {1.00f, 0.45f, 0.90f, 1.0f};
            default: return ImGui::GetStyleColorVec4(ImGuiCol_Text);
            }
        }
    }

    void OutputPanel::RebuildRows()
    {
        m_rows.clear();
        m_visibleEntries = 0;
        for (const auto& entry : m_snapshot.entries)
        {
            if (!m_filter.Matches(entry)) continue;
            ++m_visibleEntries;
            const auto seconds = std::chrono::system_clock::to_time_t(entry.time);
            std::tm local{};
            localtime_s(&local, &seconds);
            const auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(
                entry.time.time_since_epoch()).count() % 1000;
            char time[32]{};
            std::snprintf(time, sizeof(time), "%02d:%02d:%02d.%03d", local.tm_hour,
                local.tm_min, local.tm_sec, static_cast<int>(millis));
            const std::string prefix = std::string(time) + "  [" + LevelName(entry.level) +
                "] [" + entry.category + "]  ";
            // One row per physical line keeps clipping correct for multiline messages.
            size_t begin = 0;
            do
            {
                const size_t end = entry.message.find('\n', begin);
                m_rows.push_back({entry.level, (begin == 0 ? prefix : "    ") +
                    entry.message.substr(begin, end == std::string::npos ? end : end - begin)});
                if (end == std::string::npos) break;
                begin = end + 1;
            } while (begin <= entry.message.size());
        }
    }

    void OutputPanel::Draw()
    {
        if (!open) return;
        ImGui::SetNextWindowSize(ImVec2(860, 280), ImGuiCond_FirstUseEver);
        const auto* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + 10,
            viewport->WorkPos.y + (std::max)(0.0f, viewport->WorkSize.y - 290)),
            ImGuiCond_FirstUseEver);
        if (!ImGui::Begin("Output", &open))
        {
            ImGui::End();
            return;
        }

        bool dirty = Logs().ReadIfChanged(m_snapshot);
        if (dirty)
        {
            m_categories = m_filter.hiddenCategories;
            for (const auto& entry : m_snapshot.entries) m_categories.insert(entry.category);
        }

        if (ImGui::Button("Clear"))
        {
            Logs().Clear();
            Logs().ReadIfChanged(m_snapshot);
            m_categories = m_filter.hiddenCategories;
            dirty = true;
        }
        ImGui::SameLine();
        const bool copy = ImGui::Button("Copy filtered");
        ImGui::SameLine();
        ImGui::Checkbox("Auto-scroll", &m_autoScroll);
        ImGui::SameLine();
        if (ImGui::Button("Reset filters"))
        {
            m_filter = {};
            m_include[0] = m_exclude[0] = '\0';
            dirty = true;
        }
        for (size_t i = 0; i < kLogLevelCount; ++i)
        {
            if (i != 0) ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Text, LevelColor(static_cast<LogLevel>(i)));
            dirty |= ImGui::Checkbox(LevelName(static_cast<LogLevel>(i)), &m_filter.levels[i]);
            ImGui::PopStyleColor();
        }
        ImGui::SetNextItemWidth(180);
        if (ImGui::BeginCombo("Category", m_filter.hiddenCategories.empty() ? "All" : "Selected"))
        {
            if (ImGui::Selectable("All categories", m_filter.hiddenCategories.empty(),
                ImGuiSelectableFlags_DontClosePopups))
            {
                m_filter.hiddenCategories.clear();
                dirty = true;
            }
            ImGui::Separator();
            for (const auto& category : m_categories)
            {
                bool selected = !m_filter.hiddenCategories.contains(category);
                // Explicit ID keeps arbitrary category text out of ImGui's label parser.
                ImGui::PushID(category.c_str());
                if (ImGui::Checkbox("##category", &selected))
                {
                    if (!selected) m_filter.hiddenCategories.insert(category);
                    else m_filter.hiddenCategories.erase(category);
                    dirty = true;
                }
                ImGui::SameLine();
                ImGui::TextUnformatted(category.c_str());
                ImGui::PopID();
            }
            ImGui::EndCombo();
        }
        ImGui::SameLine();
        ImGui::SetNextItemWidth(180);
        if (ImGui::InputTextWithHint("##include", "Search (UTF-8)", m_include, sizeof(m_include)))
        {
            m_filter.includeText = m_include;
            dirty = true;
        }
        ImGui::SameLine();
        ImGui::SetNextItemWidth(180);
        if (ImGui::InputTextWithHint("##exclude", "Exclude", m_exclude, sizeof(m_exclude)))
        {
            m_filter.excludeText = m_exclude;
            dirty = true;
        }
        if (dirty) RebuildRows();
        ImGui::Text("%zu / %zu entries | discarded: %llu | F9: show/hide", m_visibleEntries,
            m_snapshot.entries.size(), static_cast<unsigned long long>(m_snapshot.discarded));
        if (copy)
        {
            std::string text;
            for (const auto& row : m_rows) { text += row.text; text += '\n'; }
            ImGui::SetClipboardText(text.c_str());
        }
        ImGui::Separator();
        if (ImGui::BeginChild("LogLines", ImVec2(0, 0), ImGuiChildFlags_None,
            ImGuiWindowFlags_HorizontalScrollbar))
        {
            const bool atBottom = ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 1.0f;
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
            ImGuiListClipper clipper;
            clipper.Begin(static_cast<int>(m_rows.size()));
            while (clipper.Step())
                for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i)
                {
                    const auto& row = m_rows[static_cast<size_t>(i)];
                    ImGui::PushStyleColor(ImGuiCol_Text, LevelColor(row.level));
                    ImGui::TextUnformatted(row.text.c_str());
                    ImGui::PopStyleColor();
                }
            ImGui::PopStyleVar();
            if (m_autoScroll && dirty && atBottom) ImGui::SetScrollHereY(1.0f);
        }
        ImGui::EndChild();
        ImGui::End();
    }
}
