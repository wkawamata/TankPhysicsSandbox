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
        ImGui::Begin("Renderer Settings");
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
        ImGui::End();
    }
}
