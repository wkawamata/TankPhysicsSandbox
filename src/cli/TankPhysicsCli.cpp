#include "Physics/BoxDropTest.h"
#include "Physics/MapDefinitionJson.h"
#include "Physics/TrackedVehicleTest.h"

#include <cstdlib>
#include <cstring>
#include <cmath>
#include <iostream>
#include <fstream>
#include <iterator>
#include <string>

namespace
{
    struct CliOptions
    {
        std::string testName = "box-drop";
        int steps = 300;
        float deltaTimeSeconds = 1.0f / 60.0f;
        std::string mapPath;
    };

    bool ParseArgs(int argc, char* argv[], CliOptions& options)
    {
        for (int i = 1; i < argc; i++)
        {
            if (std::strcmp(argv[i], "--test") == 0 && i + 1 < argc)
            {
                options.testName = argv[++i];
            }
            else if (std::strcmp(argv[i], "--steps") == 0 && i + 1 < argc)
            {
                options.steps = std::atoi(argv[++i]);
            }
            else if (std::strcmp(argv[i], "--dt") == 0 && i + 1 < argc)
            {
                options.deltaTimeSeconds = std::strtof(argv[++i], nullptr);
            }
            else if (std::strcmp(argv[i], "--map") == 0 && i + 1 < argc)
            {
                options.mapPath = argv[++i];
            }
            else
            {
                return false;
            }
        }

        return options.steps >= 0 && options.deltaTimeSeconds > 0.0f;
    }

    void PrintUsage()
    {
        std::cout << "Usage:\n"
                  << "  TankPhysicsCli --test box-drop --steps 300 --dt 0.0166667\n"
                  << "  TankPhysicsCli --test map --map Config/Maps/topology_course.json --steps 300 --dt 0.0166667\n";
    }

    int RunMapTest(const CliOptions& options)
    {
        if (options.mapPath.empty())
        {
            std::cerr << "Map test requires --map <path>\n";
            return 2;
        }
        std::ifstream input(options.mapPath, std::ios::binary);
        if (!input)
        {
            std::cerr << "FAIL map cannot_open=" << options.mapPath << "\n";
            return 1;
        }
        const std::istreambuf_iterator<char> begin(input);
        const std::istreambuf_iterator<char> end;
        const std::string json(begin, end);
        Tank::Physics::MapDocument document;
        std::string error;
        if (!Tank::Physics::DeserializeMapDocument(json, document, &error))
        {
            std::cerr << "FAIL map parse_error=" << error << "\n";
            return 1;
        }

        Tank::Physics::TrackedVehicleTest test;
        test.Initialize({}, document.environment, document.primitives, document.spawn);
        Tank::Physics::TrackedVehicleTestState state = test.State();
        for (int step = 0; step < options.steps; ++step)
        {
            state = test.Step(options.deltaTimeSeconds);
        }
        const bool validState =
            std::isfinite(state.bodyPosition.x) &&
            std::isfinite(state.bodyPosition.y) &&
            std::isfinite(state.bodyPosition.z) &&
            state.stepIndex == options.steps;
        if (!validState)
        {
            std::cerr << "FAIL map invalid_physics_state\n";
            return 1;
        }
        std::cout << "PASS map name=\"" << document.name
                  << "\" primitives=" << document.primitives.size()
                  << " steps=" << state.stepIndex
                  << " final_y=" << state.bodyPosition.y << "\n";
        return 0;
    }
}

int main(int argc, char* argv[])
{
    CliOptions options;
    if (!ParseArgs(argc, argv, options))
    {
        PrintUsage();
        return 2;
    }

    if (options.testName == "map")
    {
        return RunMapTest(options);
    }
    if (options.testName != "box-drop")
    {
        std::cerr << "Unknown test: " << options.testName << "\n";
        PrintUsage();
        return 2;
    }

    Tank::Physics::BoxDropTest test;
    test.Initialize();

    Tank::Physics::BoxDropState state = test.State();
    for (int i = 0; i < options.steps; i++)
    {
        state = test.Step(options.deltaTimeSeconds);
    }

    std::cout << "test=box-drop"
              << " step=" << state.stepIndex
              << " t=" << state.timeSeconds
              << " box.y=" << state.boxPosition.y
              << " sleeping=" << (state.boxSleeping ? "true" : "false")
              << "\n";

    const bool validHeight = std::isfinite(state.boxPosition.y) && state.boxPosition.y >= 0.45f && state.boxPosition.y <= 0.65f;
    if (!validHeight)
    {
        std::cout << "FAIL box-drop final_y=" << state.boxPosition.y << "\n";
        return 1;
    }

    std::cout << "PASS box-drop final_y=" << state.boxPosition.y << "\n";

    return 0;
}
