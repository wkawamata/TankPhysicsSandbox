#pragma once

#include "SnapshotUndoEdit.h"
#include "Map/MapManifest.h"

namespace Tank::App
{
    struct ManifestUndoEqual
    {
        bool operator()(const Map::Manifest& left, const Map::Manifest& right) const
        {
            std::string leftJson, rightJson, error;
            return Map::SerializeManifest(left, leftJson, error) &&
                Map::SerializeManifest(right, rightJson, error) && leftJson == rightJson;
        }
    };

    using ManifestUndoEdit = SnapshotUndoEdit<Map::Manifest, ManifestUndoEqual>;
}
