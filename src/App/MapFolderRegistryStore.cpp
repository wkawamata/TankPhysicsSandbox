#include "MapFolderRegistryStore.h"

#include <fstream>
#include <iterator>
#include <nlohmann/json.hpp>
#include <system_error>

namespace
{
    constexpr int kRegistryVersion = 1;

    std::string ToUtf8(const std::filesystem::path& path)
    {
        const std::u8string value = path.u8string();
        return { value.begin(), value.end() };
    }
}

namespace Tank::App
{
    MapFolderRegistryStore::MapFolderRegistryStore(
        const std::filesystem::path& rootDirectory)
        : m_path(rootDirectory / "map_editor_folders.json")
    {
    }

    bool MapFolderRegistryStore::Read(
        std::vector<std::filesystem::path>& folders,
        std::string& status) const
    {
        std::ifstream input(m_path, std::ios::binary);
        if (!input)
        {
            status = "No saved Map Editor folders";
            return false;
        }

        try
        {
            const nlohmann::json json = nlohmann::json::parse(input);
            if (!json.is_object() || json.value("version", 0) != kRegistryVersion ||
                !json.contains("folders") || !json.at("folders").is_array())
            {
                status = "Map folder registry has an unsupported format";
                return false;
            }

            std::vector<std::filesystem::path> loaded;
            for (const nlohmann::json& value : json.at("folders"))
            {
                if (!value.is_string() || value.get_ref<const std::string&>().empty())
                {
                    status = "Map folder registry contains an invalid path";
                    return false;
                }
                const std::string path = value.get<std::string>();
                const std::u8string utf8(path.begin(), path.end());
                loaded.emplace_back(utf8);
            }
            folders = std::move(loaded);
            status = "Loaded: " + m_path.string();
            return true;
        }
        catch (const std::exception& exception)
        {
            status = "Map folder registry load failed: " + std::string(exception.what());
            return false;
        }
    }

    bool MapFolderRegistryStore::Write(
        const std::vector<std::filesystem::path>& folders,
        std::string& status) const
    {
        std::error_code errorCode;
        std::filesystem::create_directories(m_path.parent_path(), errorCode);
        if (errorCode)
        {
            status = "Map folder registry save failed: " + errorCode.message();
            return false;
        }

        nlohmann::json json = {
            { "version", kRegistryVersion },
            { "folders", nlohmann::json::array() }
        };
        for (const std::filesystem::path& folder : folders)
            json["folders"].push_back(ToUtf8(folder));

        std::ofstream output(m_path, std::ios::binary | std::ios::trunc);
        if (!output)
        {
            status = "Map folder registry save failed: cannot open file";
            return false;
        }
        output << json.dump(2) << "\r\n";
        if (!output)
        {
            status = "Map folder registry save failed: cannot write file";
            return false;
        }
        status = "Saved: " + m_path.string();
        return true;
    }
}
