#pragma once

#include "Rendering/TankVisualSettingsJson.h"

#include <filesystem>
#include <string>

namespace Tank::App
{

    class TankVisualSettingsStore
    {
    public:
        explicit TankVisualSettingsStore(
            const std::filesystem::path& rootDirectory = "Config");

        bool Read(Tank::Rendering::TankVisualSettings& settings, std::string& status);
        bool Write(const Tank::Rendering::TankVisualSettings& settings, std::string& status);

        std::filesystem::path Path() const { return m_path; }

    private:
        std::filesystem::path m_path;
    };

}
