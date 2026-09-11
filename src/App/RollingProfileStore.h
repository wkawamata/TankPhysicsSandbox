#pragma once

#include "Physics/RollingProfile.h"

#include <filesystem>
#include <string>

namespace Tank::App
{
    class RollingProfileStore
    {
    public:
        explicit RollingProfileStore(
            int slot,
            const std::filesystem::path& rootDirectory = "Config");

        bool Read(Tank::Physics::RollingProfile& profile, std::string& status);
        bool Write(const Tank::Physics::RollingProfile& profile, std::string& status);
        std::filesystem::path Path() const { return m_path; }

        static std::filesystem::path MakePath(
            int slot,
            const std::filesystem::path& rootDirectory = "Config");

    private:
        std::filesystem::path m_path;
    };
}
