#include "RollingProfileStore.h"

#include <algorithm>
#include <fstream>
#include <system_error>

namespace Tank::App
{
    std::filesystem::path RollingProfileStore::MakePath(
        int slot,
        const std::filesystem::path& rootDirectory)
    {
        return rootDirectory /
            ("rolling_profile_slot" + std::to_string(std::clamp(slot, 0, 2) + 1) + ".json");
    }

    RollingProfileStore::RollingProfileStore(
        int slot,
        const std::filesystem::path& rootDirectory)
        : m_path(MakePath(slot, rootDirectory))
    {
    }

    bool RollingProfileStore::Read(
        Tank::Physics::RollingProfile& profile,
        std::string& status)
    {
        std::ifstream input(m_path, std::ios::binary);
        if (!input)
        {
            status = "Load failed: no rolling profile";
            return false;
        }

        const std::string json(
            (std::istreambuf_iterator<char>(input)),
            std::istreambuf_iterator<char>());
        Tank::Physics::RollingProfile loaded = profile;
        std::string error;
        if (!Tank::Physics::DeserializeRollingProfile(json, loaded, &error))
        {
            status = "Load failed: " + error;
            return false;
        }
        profile = loaded;
        status = "Loaded: " + m_path.string();
        return true;
    }

    bool RollingProfileStore::Write(
        const Tank::Physics::RollingProfile& profile,
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
        output << Tank::Physics::SerializeRollingProfile(profile);
        if (!output)
        {
            status = "Save failed: cannot write file";
            return false;
        }
        status = "Saved: " + m_path.string();
        return true;
    }
}
