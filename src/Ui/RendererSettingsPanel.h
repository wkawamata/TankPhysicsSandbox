#pragma once

#include <functional>
#include <string>

namespace Ui
{
    struct RendererSettingsPanelContext
    {
        std::string* settingsStatus = nullptr;
        std::string* screenshotStatus = nullptr;

        std::function<void()> saveSettings;
        std::function<void()> loadSettings;
        std::function<void()> resetSettings;
        std::function<void()> requestScreenshot;
    };

    void DrawRendererSettingsPanel(RendererSettingsPanelContext& ctx);
}
