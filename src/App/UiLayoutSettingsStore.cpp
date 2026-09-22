#include "UiLayoutSettingsStore.h"

#include <fstream>
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
        ReadBool(json, "gamepadInputWindowVisible", loaded.gamepadInputWindowVisible);
        ReadBool(json, "renderSettingsWindowVisible", loaded.renderSettingsWindowVisible);
        ReadBool(json, "outputWindowVisible", loaded.outputWindowVisible);
        ReadBool(json, "rollingCheatWindowVisible", loaded.rollingCheatWindowVisible);
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
            { "gamepadInputWindowVisible", settings.gamepadInputWindowVisible },
            { "renderSettingsWindowVisible", settings.renderSettingsWindowVisible },
            { "outputWindowVisible", settings.outputWindowVisible },
            { "rollingCheatWindowVisible", settings.rollingCheatWindowVisible },
        };
        std::ofstream output(m_path, std::ios::binary | std::ios::trunc);
        if (!output)
        {
            status = "UI layout save failed: cannot open file";
            return false;
        }
        output << json.dump(2) << "\r\n";
        if (!output)
        {
            status = "UI layout save failed: cannot write file";
            return false;
        }
        status = "Saved: " + m_path.string();
        return true;
    }
}
