#include "Physics/BoxDropTest.h"

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>

namespace
{
    struct CliOptions
    {
        std::string testName = "box-drop";
        int steps = 300;
        float deltaTimeSeconds = 1.0f / 60.0f;
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
            else
            {
                return false;
            }
        }

        return options.steps >= 0 && options.deltaTimeSeconds > 0.0f;
    }

    void PrintUsage()
    {
        std::cout << "Usage: TankPhysicsCli --test box-drop --steps 300 --dt 0.0166667\n";
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
    std::cout << "PENDING box-drop physics implementation\n";

    return 0;
}
