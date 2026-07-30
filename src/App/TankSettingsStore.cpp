#include "TankSettingsStore.h"

#include <fstream>
#include <ios>
#include <system_error>

namespace Tank::App
{

    namespace
    {
        constexpr const char* kLegacyTankSettingsPath = "Config/tank_physics.json";
    }

    std::filesystem::path TankSettingsStore::MakePath(int slot)
    {
        return std::filesystem::path("Config") /
            ("tank_physics_slot" + std::to_string(slot + 1) + ".json");
    }

    TankSettingsStore::TankSettingsStore(int slot)
        : m_slot(std::clamp(slot, 0, 2))
        , m_path(MakePath(m_slot))
    {
    }

    bool TankSettingsStore::Read(
        Tank::Physics::TankSettings& settings,
        std::string& status)
    {
        std::filesystem::path path = m_path;
        std::ifstream input(path, std::ios::binary);
        if (!input && m_slot == 0)
        {
            path = kLegacyTankSettingsPath;
            input = std::ifstream(path, std::ios::binary);
        }
        if (!input)
        {
            status = "Load failed: no saved settings";
            return false;
        }

        const std::string json(
            (std::istreambuf_iterator<char>(input)),
            std::istreambuf_iterator<char>());
        Tank::Physics::TankSettings loaded = settings;
        std::string error;
        if (!Tank::Physics::DeserializeTankSettings(json, loaded, &error))
        {
            status = "Load failed: " + error;
            return false;
        }

        settings = loaded;
        status = "Loaded: " + path.string();
        return true;
    }

    bool TankSettingsStore::Write(
        const Tank::Physics::TankSettings& settings,
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

        output << Tank::Physics::SerializeTankSettings(settings);
        if (!output)
        {
            status = "Save failed: cannot write file";
            return false;
        }

        status = "Saved: " + m_path.string();
        return true;
    }

}
