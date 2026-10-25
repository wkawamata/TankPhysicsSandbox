#include "MapCatalog.h"

#include <algorithm>
#include <fstream>
#include <iterator>
#include <stdexcept>

namespace Tank::Map
{
    std::string MapName(const Manifest& manifest, const std::filesystem::path& folder)
    {
        if (!manifest.name.empty()) return manifest.name;
        const auto utf8 = folder.filename().u8string();
        return std::string(utf8.begin(), utf8.end());
    }

    std::string MapListLabel(const Manifest& manifest, const std::filesystem::path& folder)
    {
        const auto utf8 = folder.filename().u8string();
        return std::string(utf8.begin(), utf8.end()) + "  -  " + MapName(manifest, folder);
    }

    bool LoadMapCatalog(const std::filesystem::path& root, std::vector<NamedMap>& maps, std::string& error)
    {
        try
        {
            std::vector<NamedMap> loaded;
            for (const auto& entry : std::filesystem::directory_iterator(root))
            {
                if (!entry.is_directory()) continue;
                const auto path = entry.path() / "Manifest.json";
                if (!std::filesystem::is_regular_file(path)) continue;
                std::ifstream input(path, std::ios::binary);
                if (!input) throw std::runtime_error("Cannot read " + path.string());
                const std::string bytes(std::istreambuf_iterator<char>(input), {});
                NamedMap map{entry.path(), {}};
                if (input.bad() || !DeserializeManifest(bytes, map.document, error))
                {
                    error = "Cannot load " + path.string() + ": " + error;
                    return false;
                }
                loaded.push_back(std::move(map));
            }
            std::sort(loaded.begin(), loaded.end(), [](const auto& left, const auto& right)
            { return left.folder.filename().wstring() < right.folder.filename().wstring(); });
            maps = std::move(loaded);
            error.clear();
            return true;
        }
        catch (const std::exception& exception)
        {
            error = exception.what();
            return false;
        }
    }

    bool ResolveMapByName(const std::vector<NamedMap>& maps, const std::string& name,
        NamedMap& result, std::string& error)
    {
        if (!IsValidMapName(name, error)) return false;
        const NamedMap* match = nullptr;
        for (const auto& map : maps)
        {
            if (MapName(map.document, map.folder) != name) continue;
            if (match)
            {
                error = "Ambiguous map name: " + name + ". Rename one of the maps.";
                return false;
            }
            match = &map;
        }
        if (!match)
        {
            error = "Map name not found: " + name;
            return false;
        }
        result = *match;
        error.clear();
        return true;
    }

    bool IsMapNameAvailable(const std::vector<NamedMap>& maps, const std::filesystem::path& folder,
        const std::string& name, std::string& error)
    {
        if (!IsValidMapName(name, error)) return false;
        for (const auto& map : maps)
        {
            if (MapName(map.document, map.folder) != name) continue;
            std::error_code code;
            if (std::filesystem::equivalent(map.folder, folder, code) && !code) continue;
            error = "Map name already used by " + map.folder.filename().string();
            return false;
        }
        error.clear();
        return true;
    }
}
