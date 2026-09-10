#pragma once

#include "MapManifest.h"

#include <optional>

namespace Tank::Map
{
    std::string MakeUniqueInstanceId(const Manifest& manifest);
    std::optional<std::string> DuplicateInstance(Manifest& manifest, const std::string& sourceId);
    bool RemoveInstance(Manifest& manifest, const std::string& instanceId);

    std::string MakeUniqueClearAreaId(const Manifest& manifest);
    std::string AddClearArea(Manifest& manifest);
    bool RemoveClearArea(Manifest& manifest, const std::string& areaId);
}
