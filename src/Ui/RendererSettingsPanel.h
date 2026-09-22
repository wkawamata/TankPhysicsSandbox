#pragma once

#include <functional>
#include <string>

namespace Ui
{
    struct RendererSettingsPanelContext
    {
        bool* windowVisible = nullptr;
        std::string* settingsStatus = nullptr;
        std::string* screenshotStatus = nullptr;
        int rendererFrameIndex = 0;
        float rendererCpuFrameTimeMs = 0.0f;

        std::function<void()> saveSettings;
        std::function<void()> loadSettings;
        std::function<void()> resetSettings;
        std::function<void()> requestScreenshot;
        std::function<void()> drawRendererDebugContents;
    };

    void DrawRendererSettingsPanel(RendererSettingsPanelContext& ctx);
}
