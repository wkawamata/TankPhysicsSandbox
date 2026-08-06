#include "Physics/MapDefinitionJson.h"
#include "Physics/TrackedVehicleTest.h"

#include <cmath>
#include <iostream>

int main()
{
    using namespace Tank::Physics;
    MapDocument source;
    source.name = "Ramp Test";
    source.environment.floorFriction = 0.7f;
    source.primitives = {
        { MapPrimitiveType::Box, { 1.0f, 2.0f, 3.0f }, { 4.0f, 1.0f, 5.0f }, 0.25f, 0.3f },
        { MapPrimitiveType::TriangularPrism, { -2.0f, 1.0f, 8.0f }, { 3.0f, 2.0f, 6.0f }, 0.0f, 1.0f },
    };
    MapPrimitive heightField;
    heightField.type = MapPrimitiveType::HeightField;
    heightField.position = { 0.0f, 0.1f, 20.0f };
    heightField.friction = 0.7f;
    heightField.heightFieldSampleCount = 3;
    heightField.heightFieldCellSizeM = 2.0f;
    heightField.heightFieldHeights = { 0.0f, 0.2f, 0.0f, 0.2f, 1.0f, 0.2f, 0.0f, 0.2f, 0.0f };
    source.primitives.push_back(heightField);

    MapDocument loaded;
    std::string error;
    if (!DeserializeMapDocument(SerializeMapDocument(source), loaded, &error) ||
        loaded.name != source.name || loaded.primitives.size() != 3 ||
        loaded.primitives[1].type != MapPrimitiveType::TriangularPrism ||
        loaded.primitives[2].heightFieldHeights.size() != 9 ||
        std::abs(loaded.environment.floorFriction - 0.7f) > 0.0001f)
    {
        std::cerr << "FAIL map JSON round trip: " << error << '\n';
        return 1;
    }

    const MapDocument before = loaded;
    if (DeserializeMapDocument(
            R"({"version":3,"name":"Future","environment":{},"primitives":[]})",
            loaded,
            &error) ||
        loaded.name != before.name || loaded.primitives.size() != before.primitives.size())
    {
        std::cerr << "FAIL future map version handling\n";
        return 1;
    }
    if (DeserializeMapDocument(
            R"({"version":1,"name":"Bad","environment":{"version":1},"primitives":[{"shape":"sphere","position":[0,0,0],"size":[1,1,1],"yawRadians":0,"friction":0.6}]})",
            loaded,
            &error) ||
        loaded.name != before.name)
    {
        std::cerr << "FAIL unknown map shape handling\n";
        return 1;
    }

    TrackedVehicleTest physicsTest;
    physicsTest.Initialize({}, source.environment, { heightField });
    physicsTest.Step(1.0f / 60.0f);

    std::cout << "PASS MapDefinitionJson\n";
    return 0;
}
