#include "MortarProfileStore.h"
#include <algorithm>
#include <fstream>
#include <system_error>

namespace Tank::App
{
    std::filesystem::path MortarProfileStore::MakePath(int slot, const std::filesystem::path& root)
    { return root / ("mortar_profile_slot" + std::to_string(std::clamp(slot, 0, 2) + 1) + ".json"); }
    MortarProfileStore::MortarProfileStore(int slot, const std::filesystem::path& root) : m_path(MakePath(slot, root)) {}
    bool MortarProfileStore::Read(Tank::Physics::MortarProfile& profile, std::string& status)
    {
        std::ifstream input(m_path, std::ios::binary);
        if (!input) { status = "Load failed: no mortar profile"; return false; }
        const std::string json((std::istreambuf_iterator<char>(input)), {});
        Tank::Physics::MortarProfile loaded = profile; std::string error;
        if (!Tank::Physics::DeserializeMortarProfile(json, loaded, &error)) { status = "Load failed: " + error; return false; }
        profile = loaded; status = "Loaded: " + m_path.string(); return true;
    }
    bool MortarProfileStore::Write(const Tank::Physics::MortarProfile& profile, std::string& status)
    {
        std::error_code error; std::filesystem::create_directories(m_path.parent_path(), error);
        if (error) { status = "Save failed: " + error.message(); return false; }
        std::ofstream output(m_path, std::ios::binary | std::ios::trunc);
        if (!output) { status = "Save failed: cannot open file"; return false; }
        output << Tank::Physics::SerializeMortarProfile(profile);
        if (!output) { status = "Save failed: cannot write file"; return false; }
        status = "Saved: " + m_path.string(); return true;
    }
}
