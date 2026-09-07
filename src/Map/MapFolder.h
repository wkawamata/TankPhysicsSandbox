#pragma once

#include "MapManifest.h"

#include <filesystem>

namespace Tank::Map
{
    // Owns one map editing session. Failed operations preserve the session.
    class MapFolder
    {
    public:
        bool Open(const std::filesystem::path& folder, std::string& error);
        bool Save(std::string& error);
        bool SetManifest(const Manifest& manifest, std::string& error);

        bool IsOpen() const { return !m_folder.empty(); }
        bool IsDirty() const { return m_currentJson != m_savedJson; }
        const std::filesystem::path& Folder() const { return m_folder; }
        const Manifest& Document() const { return m_manifest; }

    private:
        std::filesystem::path m_folder;
        Manifest m_manifest;
        std::string m_currentJson;
        std::string m_savedJson;
    };
}
