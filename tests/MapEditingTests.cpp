#include "Map/MapEditing.h"

#include <iostream>
#include <stdexcept>

namespace
{
    void Check(bool condition, const char* message)
    {
        if (!condition) throw std::runtime_error(message);
    }
}

int main()
{
    using namespace Tank::Map;
    try
    {
        Manifest manifest;
        manifest.instances = {
            { "instance-1", "Models/A.gltf", { { 3.0f, 4.0f, 5.0f }, { 10.0f, 20.0f, 30.0f } } },
            { "instance-3", "Models/B.gltf", {} }
        };
        manifest.clearAreas = { { "instance-2", "Reserved", {}, { 1.0f, 1.0f, 1.0f } } };
        Check(MakeUniqueInstanceId(manifest) == "instance-4", "IDs shared with clear areas must be skipped");

        const auto duplicateId = DuplicateInstance(manifest, "instance-1");
        Check(duplicateId && *duplicateId == "instance-4", "Duplicate should receive the first unused ID");
        const Instance& duplicate = manifest.instances.back();
        Check(duplicate.asset == "Models/A.gltf" && duplicate.transform.position[0] == 3.0f &&
            duplicate.transform.rotationDegrees[2] == 30.0f, "Duplicate should preserve asset and transform");
        Check(!DuplicateInstance(manifest, "missing"), "Missing source should not change the map");
        Check(manifest.instances.size() == 3, "Failed duplicate must preserve instances");

        Check(RemoveInstance(manifest, "instance-1"), "Existing instance should be removable");
        Check(!RemoveInstance(manifest, "instance-1"), "Removed instance should no longer exist");
        Check(manifest.instances.size() == 2, "Only the selected instance should be removed");

        manifest.instances.push_back({ "clear-area-1", "Models/C.gltf", {} });
        Check(MakeUniqueClearAreaId(manifest) == "clear-area-2",
            "Clear area IDs must skip IDs used by model instances");
        const std::string areaId = AddClearArea(manifest);
        Check(areaId == "clear-area-2" && manifest.clearAreas.back().name == "Clear Area" &&
            manifest.clearAreas.back().size == std::array<float, 3>({ 1.0f, 1.0f, 1.0f }),
            "A new clear area should have editable defaults");
        Check(RemoveClearArea(manifest, areaId), "Existing clear area should be removable");
        Check(!RemoveClearArea(manifest, areaId), "Removed clear area should no longer exist");
        std::cout << "Map editing tests passed\n";
        return 0;
    }
    catch (const std::exception& exception)
    {
        std::cerr << exception.what() << '\n';
        return 1;
    }
}
