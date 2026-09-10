#include "MapEditing.h"

#include <algorithm>

namespace Tank::Map
{
    namespace
    {
        bool IdIsUsed(const Manifest& manifest, const std::string& candidate)
        {
            const bool usedByInstance = std::any_of(manifest.instances.begin(), manifest.instances.end(),
                [&candidate](const Instance& instance) { return instance.id == candidate; });
            const bool usedByArea = std::any_of(manifest.clearAreas.begin(), manifest.clearAreas.end(),
                [&candidate](const ClearArea& area) { return area.id == candidate; });
            return usedByInstance || usedByArea;
        }
    }

    std::string MakeUniqueInstanceId(const Manifest& manifest)
    {
        for (size_t number = 1;; ++number)
        {
            const std::string candidate = "instance-" + std::to_string(number);
            if (!IdIsUsed(manifest, candidate)) return candidate;
        }
    }

    std::optional<std::string> DuplicateInstance(Manifest& manifest, const std::string& sourceId)
    {
        const auto source = std::find_if(manifest.instances.begin(), manifest.instances.end(),
            [&sourceId](const Instance& instance) { return instance.id == sourceId; });
        if (source == manifest.instances.end()) return std::nullopt;

        Instance duplicate = *source;
        duplicate.id = MakeUniqueInstanceId(manifest);
        manifest.instances.push_back(std::move(duplicate));
        return manifest.instances.back().id;
    }

    bool RemoveInstance(Manifest& manifest, const std::string& instanceId)
    {
        const auto remove = std::find_if(manifest.instances.begin(), manifest.instances.end(),
            [&instanceId](const Instance& instance) { return instance.id == instanceId; });
        if (remove == manifest.instances.end()) return false;
        manifest.instances.erase(remove);
        return true;
    }

    std::string MakeUniqueClearAreaId(const Manifest& manifest)
    {
        for (size_t number = 1;; ++number)
        {
            const std::string candidate = "clear-area-" + std::to_string(number);
            if (!IdIsUsed(manifest, candidate)) return candidate;
        }
    }

    std::string AddClearArea(Manifest& manifest)
    {
        ClearArea area;
        area.id = MakeUniqueClearAreaId(manifest);
        area.name = "Clear Area";
        manifest.clearAreas.push_back(std::move(area));
        return manifest.clearAreas.back().id;
    }

    bool RemoveClearArea(Manifest& manifest, const std::string& areaId)
    {
        const auto remove = std::find_if(manifest.clearAreas.begin(), manifest.clearAreas.end(),
            [&areaId](const ClearArea& area) { return area.id == areaId; });
        if (remove == manifest.clearAreas.end()) return false;
        manifest.clearAreas.erase(remove);
        return true;
    }
}
