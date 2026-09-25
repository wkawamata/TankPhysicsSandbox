#include "UiLayoutSettingsStore.h"

#include <fstream>
#include <algorithm>
#include <cmath>
#include <iterator>
#include <nlohmann/json.hpp>
#include <system_error>

namespace
{
    constexpr int kUiLayoutSettingsVersion = 1;

    void ReadBool(const nlohmann::json& json, const char* name, bool& value)
    {
        const auto entry = json.find(name);
        if (entry != json.end() && entry->is_boolean())
        {
            value = entry->get<bool>();
        }
    }
}

namespace Tank::App
{
    UiLayoutSettingsStore::UiLayoutSettingsStore(
        const std::filesystem::path& rootDirectory)
        : m_path(rootDirectory / "ui_layout.json")
    {
    }

    bool UiLayoutSettingsStore::Read(
        UiLayoutSettings& settings, std::string& status) const
    {
        std::ifstream input(m_path, std::ios::binary);
        if (!input)
        {
            status = "No saved UI layout";
            return false;
        }

        const std::string jsonText(
            (std::istreambuf_iterator<char>(input)),
            std::istreambuf_iterator<char>());
        const nlohmann::json json =
            nlohmann::json::parse(jsonText, nullptr, false);
        if (json.is_discarded() || !json.is_object())
        {
            status = "UI layout load failed: invalid JSON";
            return false;
        }
        if (json.value("version", 0) != kUiLayoutSettingsVersion)
        {
            status = "UI layout load failed: unsupported version";
            return false;
        }

        UiLayoutSettings loaded = settings;
        ReadBool(json, "cameraWindowVisible", loaded.cameraWindowVisible);
        ReadBool(json, "telemetryWindowVisible", loaded.telemetryWindowVisible);
        ReadBool(json, "rollingParametersWindowVisible", loaded.rollingParametersWindowVisible);
        ReadBool(json, "gamepadInputWindowVisible", loaded.gamepadInputWindowVisible);
        ReadBool(json, "renderSettingsWindowVisible", loaded.renderSettingsWindowVisible);
        ReadBool(json, "outputWindowVisible", loaded.outputWindowVisible);
        ReadBool(json, "rollingCheatWindowVisible", loaded.rollingCheatWindowVisible);
        const auto cheatFontScale = json.find("rollingCheatFontScale");
        if (cheatFontScale != json.end() && cheatFontScale->is_number())
        {
            const float scale = cheatFontScale->get<float>();
            if (std::isfinite(scale))
            {
                loaded.rollingCheatFontScale = std::clamp(scale, 0.5f, 2.0f);
            }
        }
        settings = loaded;
        status = "Loaded: " + m_path.string();
        return true;
    }

    bool UiLayoutSettingsStore::Write(
        const UiLayoutSettings& settings, std::string& status) const
    {
        std::error_code errorCode;
        std::filesystem::create_directories(m_path.parent_path(), errorCode);
        if (errorCode)
        {
            status = "UI layout save failed: " + errorCode.message();
            return false;
        }

        const nlohmann::json json = {
            { "version", kUiLayoutSettingsVersion },
            { "cameraWindowVisible", settings.cameraWindowVisible },
            { "telemetryWindowVisible", settings.telemetryWindowVisible },
            { "rollingParametersWindowVisible", settings.rollingParametersWindowVisible },
            { "gamepadInputWindowVisible", settings.gamepadInputWindowVisible },
            { "renderSettingsWindowVisible", settings.renderSettingsWindowVisible },
            { "outputWindowVisible", settings.outputWindowVisible },
            { "rollingCheatWindowVisible", settings.rollingCheatWindowVisible },
            { "rollingCheatFontScale", settings.rollingCheatFontScale },
        };
        std::string jsonText = json.dump(2);
        std::string crlfJsonText;
        crlfJsonText.reserve(jsonText.size() + 16);
        for (const char character : jsonText)
        {
            if (character == '\n')
            {
                crlfJsonText += "\r\n";
            }
            else
            {
                crlfJsonText += character;
            }
        }
        crlfJsonText += "\r\n";

        std::ofstream output(m_path, std::ios::binary | std::ios::trunc);
        if (!output)
        {
            status = "UI layout save failed: cannot open file";
            return false;
        }
        output << crlfJsonText;
        if (!output)
        {
            status = "UI layout save failed: cannot write file";
            return false;
        }
        status = "Saved: " + m_path.string();
        return true;
    }
}
