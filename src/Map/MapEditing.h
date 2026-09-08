#pragma once

#include "MapManifest.h"

#include <optional>

namespace Tank::Map
{
    std::string MakeUniqueInstanceId(const Manifest& manifest);
    std::optional<std::string> DuplicateInstance(Manifest& manifest, const std::string& sourceId);
    bool RemoveInstance(Manifest& manifest, const std::string& instanceId);
}
