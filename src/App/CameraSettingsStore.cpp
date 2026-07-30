#include "CameraSettingsStore.h"

#include <fstream>
#include <ios>
#include <system_error>

namespace Tank::App
{

    std::filesystem::path CameraSettingsStore::MakePath(int slot)
    {
        if (slot == 3)
        {
            return std::filesystem::path("Config") / "camera_debug.json";
        }
        return std::filesystem::path("Config") /
            ("camera_slot" + std::to_string(slot + 1) + ".json");
    }

    CameraSettingsStore::CameraSettingsStore(int slot)
        : m_slot(std::clamp(slot, 0, 3))
        , m_path(MakePath(m_slot))
    {
    }

    bool CameraSettingsStore::Read(
        Tank::Rendering::CameraSettings& settings,
        std::string& status)
    {
        std::ifstream input(m_path, std::ios::binary);
        if (!input)
        {
            status = "No saved camera in slot";
            return false;
        }

        const std::string json(
            (std::istreambuf_iterator<char>(input)),
            std::istreambuf_iterator<char>());
        std::string error;
        if (!Tank::Rendering::DeserializeCameraSettings(json, settings, &error))
        {
            status = "Load failed: " + error;
            return false;
        }

        status = m_slot == 3
            ? "Loaded debug camera"
            : "Loaded slot " + std::to_string(m_slot + 1);
        return true;
    }

    bool CameraSettingsStore::Write(
        const Tank::Rendering::CameraSettings& settings,
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
            status = "Save failed: cannot write file";
            return false;
        }
        output << Tank::Rendering::SerializeCameraSettings(settings);
        if (!output)
        {
            status = "Save failed: write error";
            return false;
        }

        status = "Saved: " + m_path.string();
        return true;
    }

}
