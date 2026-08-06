#include "Physics/BoxDropTest.h"
#include "Physics/MapDefinitionJson.h"
#include "Physics/TrackedVehicleTest.h"

#include <cstdlib>
#include <cstring>
#include <cmath>
#include <iostream>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>

namespace
{
    struct CliOptions
    {
        std::string testName = "box-drop";
        int steps = 300;
        int settleSteps = 180;
        float deltaTimeSeconds = 1.0f / 60.0f;
        std::string mapPath;
        float throttle = 0.0f;
        float leftTrack = 1.0f;
        float rightTrack = 1.0f;
        float minimumForwardDistance = 0.0f;
        std::optional<float> minimumFinalY;
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
            else if (std::strcmp(argv[i], "--settle-steps") == 0 && i + 1 < argc)
            {
                options.settleSteps = std::atoi(argv[++i]);
            }
            else if (std::strcmp(argv[i], "--map") == 0 && i + 1 < argc)
            {
                options.mapPath = argv[++i];
            }
            else if (std::strcmp(argv[i], "--throttle") == 0 && i + 1 < argc)
            {
                options.throttle = std::strtof(argv[++i], nullptr);
            }
            else if (std::strcmp(argv[i], "--left-track") == 0 && i + 1 < argc)
            {
                options.leftTrack = std::strtof(argv[++i], nullptr);
            }
            else if (std::strcmp(argv[i], "--right-track") == 0 && i + 1 < argc)
            {
                options.rightTrack = std::strtof(argv[++i], nullptr);
            }
            else if (std::strcmp(argv[i], "--min-forward-distance") == 0 && i + 1 < argc)
            {
                options.minimumForwardDistance = std::strtof(argv[++i], nullptr);
            }
            else if (std::strcmp(argv[i], "--min-final-y") == 0 && i + 1 < argc)
            {
                options.minimumFinalY = std::strtof(argv[++i], nullptr);
            }
            else
            {
                return false;
            }
        }

        return options.steps >= 0 && options.settleSteps >= 0 &&
            options.deltaTimeSeconds > 0.0f &&
            std::isfinite(options.throttle) && options.throttle >= -1.0f &&
            options.throttle <= 1.0f &&
            std::isfinite(options.leftTrack) && options.leftTrack >= -1.0f &&
            options.leftTrack <= 1.0f &&
            std::isfinite(options.rightTrack) && options.rightTrack >= -1.0f &&
            options.rightTrack <= 1.0f &&
            std::isfinite(options.minimumForwardDistance) &&
            options.minimumForwardDistance >= 0.0f &&
            (!options.minimumFinalY || std::isfinite(*options.minimumFinalY));
    }

    void PrintUsage()
    {
        std::cout << "Usage:\n"
                  << "  TankPhysicsCli --test box-drop --steps 300 --dt 0.0166667\n"
                  << "  TankPhysicsCli --test map --map Config/Maps/topology_course.json --settle-steps 180 --steps 300 --throttle 1 --min-forward-distance 5 --min-final-y 0\n";
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
        for (int step = 0; step < options.settleSteps; ++step)
        {
            state = test.Step(options.deltaTimeSeconds);
        }
        const Tank::Physics::Vec3 startPosition = state.bodyPosition;
        Tank::Physics::TankInput inputState;
        inputState.throttle = options.throttle;
        inputState.leftTrack = options.leftTrack;
        inputState.rightTrack = options.rightTrack;
        test.SetInput(inputState);
        for (int step = 0; step < options.steps; ++step)
        {
            state = test.Step(options.deltaTimeSeconds);
        }
        const bool validState =
            std::isfinite(state.bodyPosition.x) &&
            std::isfinite(state.bodyPosition.y) &&
            std::isfinite(state.bodyPosition.z) &&
            state.stepIndex == options.settleSteps + options.steps;
        const float forwardX = std::sin(document.spawn.yawRadians);
        const float forwardZ = std::cos(document.spawn.yawRadians);
        const float forwardDistance =
            (state.bodyPosition.x - startPosition.x) * forwardX +
            (state.bodyPosition.z - startPosition.z) * forwardZ;
        if (!validState)
        {
            std::cerr << "FAIL map invalid_physics_state\n";
            return 1;
        }
        if (forwardDistance < options.minimumForwardDistance)
        {
            std::cerr << "FAIL map forward_distance=" << forwardDistance
                      << " required=" << options.minimumForwardDistance << "\n";
            return 1;
        }
        if (options.minimumFinalY && state.bodyPosition.y < *options.minimumFinalY)
        {
            std::cerr << "FAIL map final_y=" << state.bodyPosition.y
                      << " required=" << *options.minimumFinalY << "\n";
            return 1;
        }
        std::cout << "PASS map name=\"" << document.name
                  << "\" primitives=" << document.primitives.size()
                  << " steps=" << options.steps
                  << " forward_distance=" << forwardDistance
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
