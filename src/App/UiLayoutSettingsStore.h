#pragma once

#include <filesystem>
#include <string>

namespace Tank::App
{
    // Host-owned visibility state. ImGui keeps placement and collapsed regions.
    struct UiLayoutSettings
    {
        bool cameraWindowVisible = true;
        bool telemetryWindowVisible = true;
        bool rollingParametersWindowVisible = true;
        bool gamepadInputWindowVisible = true;
        bool renderSettingsWindowVisible = true;
        bool outputWindowVisible = true;
        bool rollingCheatWindowVisible = false;
        float rollingCheatFontScale = 1.0f;

        bool operator==(const UiLayoutSettings&) const = default;
    };

    class UiLayoutSettingsStore
    {
    public:
        explicit UiLayoutSettingsStore(
            const std::filesystem::path& rootDirectory = "Config");

        bool Read(UiLayoutSettings& settings, std::string& status) const;
        bool Write(const UiLayoutSettings& settings, std::string& status) const;

        const std::filesystem::path& Path() const { return m_path; }

    private:
        std::filesystem::path m_path;
    };
}
