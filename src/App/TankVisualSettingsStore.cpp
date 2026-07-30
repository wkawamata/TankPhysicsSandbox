#include "TankVisualSettingsStore.h"

#include <fstream>
#include <ios>
#include <system_error>

namespace Tank::App
{

    TankVisualSettingsStore::TankVisualSettingsStore(
        const std::filesystem::path& rootDirectory)
        : m_path(rootDirectory / "tank_visual.json")
    {
    }

    bool TankVisualSettingsStore::Read(
        Tank::Rendering::TankVisualSettings& settings,
        std::string& status)
    {
        std::ifstream input(m_path, std::ios::binary);
        if (!input)
        {
            status = "Load failed: no saved settings";
            return false;
        }

        const std::string json(
            (std::istreambuf_iterator<char>(input)),
            std::istreambuf_iterator<char>());
        Tank::Rendering::TankVisualSettings loaded = settings;
        std::string error;
        if (!Tank::Rendering::DeserializeTankVisualSettings(json, loaded, &error))
        {
            status = "Load failed: " + error;
            return false;
        }

        settings = loaded;
        status = std::string("Loaded: ") + m_path.string();
        return true;
    }

    bool TankVisualSettingsStore::Write(
        const Tank::Rendering::TankVisualSettings& settings,
        std::string& status)
    {
        std::error_code errorCode;
        std::filesystem::create_directories(m_path.parent_path(), errorCode);
        if (errorCode)
        {
            status = "Save failed: " + errorCode.message();
            return false;
        }

        std::ofstream output(m_path, std::ios::binary | std::ios::trunc);
        if (!output)
        {
            status = "Save failed: cannot open file";
            return false;
        }
        output << Tank::Rendering::SerializeTankVisualSettings(settings);
        if (!output)
        {
            status = "Save failed: cannot write file";
            return false;
        }

        status = std::string("Saved: ") + m_path.string();
        return true;
    }

}
