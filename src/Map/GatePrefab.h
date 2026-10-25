#pragma once
#include "MapEditing.h"
#include <algorithm>
#include "MapHierarchy.h"

namespace Tank::Map
{
    inline constexpr float GateWidths[] = {10, 5, 20};
    inline constexpr float GateHeights[] = {3, 3, 5};

    // Assets are copied into the map before calling this pure document operation.
    inline std::string AddGatePrefab(Manifest& document, int variant)
    {
        if (variant < 0 || variant >= 3) return {};
        const auto tree = MapTree(document);
        std::string group;
        for (int n = 1;; ++n)
        {
            group = "gate-" + std::to_string(n);
            if (std::none_of(tree.begin(), tree.end(), [&](const auto& node) {return node.id == group;})) break;
        }
        const std::string size = std::to_string(static_cast<int>(GateWidths[variant]));
        document.nodes.push_back({group, "Gate " + size + "m", "root", {}});
        Instance frame;
        frame.id = MakeUniqueInstanceId(document);
        frame.parentId = group;
        frame.asset = "Models/Gate" + size + "Frame.gltf";
        document.nodeInfo["mesh/" + frame.id].name = "Gate frame";
        document.instances.push_back(frame);
        Instance shutter;
        shutter.id = MakeUniqueInstanceId(document);
        shutter.parentId = group;
        shutter.asset = "Models/Gate" + size + "Shutter.gltf";
        shutter.gate = GateSettings{12, GateHeights[variant] + 0.2f, 2};
        document.nodeInfo["mesh/" + shutter.id].name = "Gate shutter";
        document.instances.push_back(shutter);
        return group;
    }
}
