#pragma once

#include "Rendering/CameraSettings.h"

#include <filesystem>
#include <string>

namespace Tank::App
{

    class CameraSettingsStore
    {
    public:
        explicit CameraSettingsStore(
            int slot,
            const std::filesystem::path& rootDirectory = "Config");

        bool Read(Tank::Rendering::CameraSettings& settings, std::string& status);
        bool Write(const Tank::Rendering::CameraSettings& settings, std::string& status);

        int Slot() const { return m_slot; }
        std::filesystem::path Path() const { return m_path; }

        static std::filesystem::path MakePath(
            int slot,
            const std::filesystem::path& rootDirectory = "Config");

    private:
        int m_slot;
        std::filesystem::path m_path;
    };

}
