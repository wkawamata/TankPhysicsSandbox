#include "MapEditing.h"

#include <algorithm>

namespace Tank::Map
{
    std::string MakeUniqueInstanceId(const Manifest& manifest)
    {
        const auto idIsUsed = [&manifest](const std::string& candidate)
        {
            const bool usedByInstance = std::any_of(manifest.instances.begin(), manifest.instances.end(),
                [&candidate](const Instance& instance) { return instance.id == candidate; });
            const bool usedByArea = std::any_of(manifest.clearAreas.begin(), manifest.clearAreas.end(),
                [&candidate](const ClearArea& area) { return area.id == candidate; });
            return usedByInstance || usedByArea;
        };

        for (size_t number = 1;; ++number)
        {
            const std::string candidate = "instance-" + std::to_string(number);
            if (!idIsUsed(candidate)) return candidate;
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
}
