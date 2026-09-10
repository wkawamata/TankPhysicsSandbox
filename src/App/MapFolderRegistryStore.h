#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace Tank::App
{
    class MapFolderRegistryStore
    {
    public:
        explicit MapFolderRegistryStore(
            const std::filesystem::path& rootDirectory = "Config");

        bool Read(std::vector<std::filesystem::path>& folders, std::string& status) const;
        bool Write(const std::vector<std::filesystem::path>& folders, std::string& status) const;

        const std::filesystem::path& Path() const { return m_path; }

    private:
        std::filesystem::path m_path;
    };
}
