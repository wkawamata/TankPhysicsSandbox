#pragma once

#include "Physics/MortarProfile.h"
#include <filesystem>
#include <string>

namespace Tank::App
{
    class MortarProfileStore
    {
    public:
        explicit MortarProfileStore(int slot, const std::filesystem::path& rootDirectory = "Config");
        bool Read(Tank::Physics::MortarProfile& profile, std::string& status);
        bool Write(const Tank::Physics::MortarProfile& profile, std::string& status);
        static std::filesystem::path MakePath(int slot, const std::filesystem::path& rootDirectory = "Config");
    private:
        std::filesystem::path m_path;
    };
}
