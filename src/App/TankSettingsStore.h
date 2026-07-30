#pragma once

#include "Physics/TankSettingsJson.h"
#include "Physics/TankTypes.h"

#include <filesystem>
#include <string>

namespace Tank::App
{

    class TankSettingsStore
    {
    public:
        explicit TankSettingsStore(int slot);

        bool Read(Tank::Physics::TankSettings& settings, std::string& status);
        bool Write(const Tank::Physics::TankSettings& settings, std::string& status);

        int Slot() const { return m_slot; }
        std::filesystem::path Path() const { return m_path; }

        static std::filesystem::path MakePath(int slot);

    private:
        int m_slot;
        std::filesystem::path m_path;
    };

}
