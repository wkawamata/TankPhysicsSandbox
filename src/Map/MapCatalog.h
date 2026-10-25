#pragma once

#include "MapManifest.h"

#include <filesystem>
#include <vector>

namespace Tank::Map
{
    struct NamedMap
    {
        std::filesystem::path folder;
        Manifest document;
    };

    // Names are exact, case-sensitive UTF-8 keys, independent of storage paths.
    std::string MapName(const Manifest& manifest, const std::filesystem::path& folder);
    std::string MapListLabel(const Manifest& manifest, const std::filesystem::path& folder);
    bool LoadMapCatalog(const std::filesystem::path& root, std::vector<NamedMap>& maps, std::string& error);
    bool ResolveMapByName(const std::vector<NamedMap>& maps, const std::string& name,
        NamedMap& result, std::string& error);
    bool IsMapNameAvailable(const std::vector<NamedMap>& maps, const std::filesystem::path& folder,
        const std::string& name, std::string& error);
}
