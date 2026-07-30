#pragma once

#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

namespace Engine { class Scene; }
namespace Tank::Physics { struct TrackedVehicleTestState; }

namespace Tank::Rendering
{
    struct TankExportPart
    {
        size_t instanceIndex = 0;
        std::string name;
    };

    bool ExportTankGlb(
        const Engine::Scene& scene,
        const std::vector<TankExportPart>& parts,
        const Tank::Physics::TrackedVehicleTestState& state,
        const std::filesystem::path& path,
        std::string& status);
}
