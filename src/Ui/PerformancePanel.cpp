#include "PerformancePanel.h"
#include "App/PerformanceMeter.h"
#include "imgui.h"
#include <cstdio>

namespace Ui
{
    namespace
    {
        void BudgetBar(const char* label, const Tank::App::TimingStats& stats)
        {
            constexpr float budget = 1000.0f / 60;
            char text[128];
            std::snprintf(text, sizeof(text), "%s: %.2f ms / 16.67 ms", label, stats.average);
            ImGui::PushStyleColor(ImGuiCol_PlotHistogram, stats.average > budget
                ? ImVec4(.9f,.3f,.2f,1) : ImVec4(.25f,.65f,.8f,1));
            ImGui::ProgressBar(std::min(1.0f, stats.average / budget), ImVec2(-1, 0), text);
            ImGui::PopStyleColor();
        }
        void TimingRow(const char* name, const Tank::App::TimingStats& time, float total)
        {
            ImGui::TableNextRow(); ImGui::TableNextColumn(); ImGui::TextUnformatted(name);
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", name);
            for (float value : {time.latest, time.average, time.p95, time.peak})
            { ImGui::TableNextColumn(); ImGui::Text("%.2f", value); }
            ImGui::TableNextColumn();
            const float fraction = total > 0 ? std::clamp(time.average / total, 0.0f, 1.0f) : 0;
            char label[24]; std::snprintf(label, sizeof(label), "%.0f%%", fraction * 100);
            ImGui::ProgressBar(fraction, ImVec2(-1, 0), label);
        }
        void Columns()
        {
            ImGui::TableSetupColumn("Process", ImGuiTableColumnFlags_WidthStretch, 3.5f);
            for (const char* label : {"Last", "Avg", "P95", "Peak", "Share"})
                ImGui::TableSetupColumn(label, ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableHeadersRow();
        }
    }

    void DrawPerformancePanel(const Tank::App::PerformanceHistory& history)
    {
        ImGui::SetNextWindowPos(ImVec2(615, 40), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(570, 800), ImGuiCond_FirstUseEver);
        if (!ImGui::Begin("Performance Meter")) { ImGui::End(); return; }
        const auto& data = history.Summary();
        if (!data.frames) { ImGui::TextUnformatted("Collecting frame timings..."); ImGui::End(); return; }
        ImGui::Text("%.1f FPS | frame avg %.2f ms | p95 %.2f | peak %.2f",
            data.interval.average > 0 ? 1000 / data.interval.average : 0,
            data.interval.average, data.interval.p95, data.interval.peak);
        ImGui::TextDisabled("Rolling %zu / 300 frames; refreshed every 15 frames. Times in ms.", data.frames);
        ImGui::TextDisabled("Outside host tick / event loop: %.2f ms (approx avg)",
            std::max(0.0f, data.interval.average - data.host.average));
        BudgetBar("Host elapsed", data.host);
        if (data.gpuFrames) BudgetBar("GPU rendering", data.gpu);
        else ImGui::TextDisabled("GPU: waiting for completed timestamp queries");
        ImGui::TextWrapped("60 FPS reference. CPU/host and GPU overlap; do not add these bars. Host elapsed includes waits, not CPU utilization.");
        if (ImGui::CollapsingHeader("Tank + renderer CPU elapsed", ImGuiTreeNodeFlags_DefaultOpen))
        {
            std::array<size_t, Tank::App::kCpuPhaseCount> order;
            std::iota(order.begin(), order.end(), 0);
            std::stable_sort(order.begin(), order.end(), [&](size_t a, size_t b)
                { return data.cpu[a].average > data.cpu[b].average; });
            ImGui::Text("Largest host interval: %s", Tank::App::kCpuPhaseNames[order.front()]);
            const auto tank = std::find_if(order.begin(), order.end(), [](size_t phase)
                { return phase != static_cast<size_t>(Tank::App::CpuPhase::Renderer); });
            ImGui::Text("Largest Tank interval: %s", Tank::App::kCpuPhaseNames[*tank]);
            if (ImGui::BeginTable("CpuTimings", 6, ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable))
            {
                Columns();
                for (auto index : order) TimingRow(Tank::App::kCpuPhaseNames[index], data.cpu[index], data.host.average);
                ImGui::EndTable();
            }
            ImGui::TextWrapped("Physics includes vehicle/enemy/projectile gameplay and Jolt workers. Scene sync includes transforms, debug lines and renderer SetScene. Renderer includes command recording, GPU synchronization and Present/VSync waits.");
        }
        if (ImGui::CollapsingHeader("GPU pass breakdown (RtPbr)", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::Text("Completed GPU samples: %zu (readback delayed by in-flight frames)", data.gpuFrames);
            if (ImGui::BeginTable("GpuTimings", 6, ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable))
            {
                Columns();
                for (const auto& pass : data.gpuPasses) TimingRow(pass.name.c_str(), pass.time, data.gpu.average);
                ImGui::EndTable();
            }
        }
        ImGui::TextDisabled("Collapse this window or hide tool UI with left + right mouse buttons.");
        ImGui::End();
    }
}
