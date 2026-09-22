#include "stdafx.h"
#include "Ui/RendererSettingsPanel.h"

#include "imgui.h"

namespace Ui
{
    namespace
    {
        constexpr const char* kRendererSettingsPath = "Config/renderer_debug.json";
    }

    void DrawRendererSettingsPanel(RendererSettingsPanelContext& ctx)
    {
        if (ctx.windowVisible == nullptr || !*ctx.windowVisible)
        {
            return;
        }

        ImGui::SetNextWindowSize(ImVec2(440.0f, 700.0f), ImGuiCond_FirstUseEver);
        if (!ImGui::Begin("Render Settings", ctx.windowVisible))
        {
            ImGui::End();
            return;
        }

        ImGui::Text("FrameIndex: %d", ctx.rendererFrameIndex);
        if (ctx.rendererCpuFrameTimeMs > 0.0f)
        {
            ImGui::Text("CPU Frame: %.2f ms (%.1f FPS)",
                ctx.rendererCpuFrameTimeMs,
                1000.0f / ctx.rendererCpuFrameTimeMs);
        }
        else
        {
            ImGui::TextUnformatted("CPU Frame: unavailable");
        }
        ImGui::Separator();
        ImGui::TextUnformatted(kRendererSettingsPath);
        if (ImGui::Button("Save"))
        {
            if (ctx.saveSettings) ctx.saveSettings();
        }
        ImGui::SameLine();
        if (ImGui::Button("Load"))
        {
            if (ctx.loadSettings) ctx.loadSettings();
        }
        ImGui::SameLine();
        if (ImGui::Button("Reset"))
        {
            if (ctx.resetSettings) ctx.resetSettings();
        }
        if (ctx.settingsStatus && !ctx.settingsStatus->empty())
        {
            ImGui::TextWrapped("%s", ctx.settingsStatus->c_str());
        }
        ImGui::Separator();
        if (ImGui::Button("Capture"))
        {
            if (ctx.requestScreenshot) ctx.requestScreenshot();
        }
        ImGui::SameLine();
        ImGui::TextUnformatted("F12");
        if (ctx.screenshotStatus && !ctx.screenshotStatus->empty())
        {
            ImGui::TextWrapped("%s", ctx.screenshotStatus->c_str());
        }

        ImGui::Separator();
        if (ImGui::BeginChild(
            "RtPbrSurveyDebugContents",
            ImVec2(0.0f, 0.0f),
            ImGuiChildFlags_Borders))
        {
            if (ctx.drawRendererDebugContents)
            {
                ctx.drawRendererDebugContents();
            }
        }
        ImGui::EndChild();
        ImGui::End();
    }
}
