#include "MapAssetCatalog.h"

#define NOMINMAX
#include <Windows.h>

#include <algorithm>
#include <stdexcept>

namespace Tank::Map
{
    bool FindMapAssets(const std::filesystem::path& folder,
        std::vector<std::string>& assets, std::string& error)
    {
        try
        {
            if (folder.empty() || !std::filesystem::is_directory(folder))
                throw std::runtime_error("Map folder does not exist or is not a directory.");
            const auto root = std::filesystem::canonical(folder);
            std::vector<std::string> found;
            for (auto it = std::filesystem::recursive_directory_iterator(root);
                it != std::filesystem::recursive_directory_iterator(); ++it)
            {
                const auto& entry = *it;
                // Windows junctions must also be excluded, not just symbolic links.
                const DWORD attributes = GetFileAttributesW(entry.path().c_str());
                if (attributes == INVALID_FILE_ATTRIBUTES)
                    throw std::runtime_error("Cannot inspect a map folder entry (Windows error " +
                        std::to_string(GetLastError()) + ").");
                if ((attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0)
                {
                    it.disable_recursion_pending();
                    continue;
                }
                if (!entry.is_regular_file()) continue;
                auto extension = entry.path().extension().wstring();
                for (auto& c : extension)
                    if (c >= L'A' && c <= L'Z') c += L'a' - L'A';
                if (extension != L".gltf" && extension != L".glb") continue;
                const auto path = entry.path().lexically_relative(root).generic_u8string();
                found.emplace_back(path.begin(), path.end());
            }
            std::sort(found.begin(), found.end());
            assets = std::move(found);
            error.clear();
            return true;
        }
        catch (const std::exception& exception)
        {
            error = exception.what();
            return false;
        }
    }
}
