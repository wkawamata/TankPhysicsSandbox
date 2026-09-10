#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace Tank::Map
{
    // Sorted UTF-8 paths relative to the map folder, suitable for Manifest assets.
    // Searches subfolders, excluding links/reparse points. Does not parse glTF.
    // On failure, assets remains unchanged and error describes the failure.
    bool FindMapAssets(const std::filesystem::path& folder,
        std::vector<std::string>& assets, std::string& error);
}
