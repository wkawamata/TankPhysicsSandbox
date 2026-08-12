#include "Physics/BoxDropTest.h"
#include "Physics/MapDefinitionJson.h"
#include "Physics/TankSettingsJson.h"
#include "Physics/TrackedVehicleTest.h"

#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <vector>

namespace
{
    struct CliOptions
    {
        std::string testName = "box-drop";
        int steps = 300;
        int settleSteps = 180;
        float deltaTimeSeconds = 1.0f / 60.0f;
        std::string mapPath;
        std::string mapDirectory;
        std::string tankSettingsPath;
        std::string tankSettingsDirectory;
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
            else if (std::strcmp(argv[i], "--map-directory") == 0 && i + 1 < argc)
            {
                options.mapDirectory = argv[++i];
            }
            else if (std::strcmp(argv[i], "--tank-settings") == 0 && i + 1 < argc)
            {
                options.tankSettingsPath = argv[++i];
            }
            else if (std::strcmp(argv[i], "--tank-settings-directory") == 0 && i + 1 < argc)
            {
                options.tankSettingsDirectory = argv[++i];
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
                  << "  TankPhysicsCli --test maps --map-directory Config/Maps\n"
                  << "  TankPhysicsCli --test mobility-suite --map Config/Maps/mobility_course.json --tank-settings-directory tests/data/mobility --steps 480 --throttle 1\n"
                  << "  TankPhysicsCli --test map --map Config/Maps/topology_course.json --tank-settings Config/Tank/tank_1.json --settle-steps 180 --steps 300 --throttle 1 --min-forward-distance 5 --min-final-y 0\n";
    }

    bool LoadTankSettings(
        const std::string& path,
        Tank::Physics::TankSettings& settings,
        std::string& error)
    {
        if (path.empty())
        {
            return true;
        }
        std::ifstream input(path, std::ios::binary);
        if (!input)
        {
            error = "cannot open tank settings";
            return false;
        }
        const std::istreambuf_iterator<char> begin(input);
        const std::istreambuf_iterator<char> end;
        return Tank::Physics::DeserializeTankSettings(
            std::string(begin, end),
            settings,
            &error);
    }

    bool LoadMapDocument(const std::filesystem::path& path, Tank::Physics::MapDocument& document, std::string& error)
    {
        std::ifstream input(path, std::ios::binary);
        if (!input)
        {
            error = "cannot open file";
            return false;
        }

        const std::istreambuf_iterator<char> begin(input);
        const std::istreambuf_iterator<char> end;
        return Tank::Physics::DeserializeMapDocument(std::string(begin, end), document, &error);
    }

    struct MobilityResult
    {
        Tank::Physics::TrackedVehicleTestState state = {};
        float forwardDistance = 0.0f;
    };

    MobilityResult EvaluateMobility(
        const CliOptions& options,
        const Tank::Physics::MapDocument& document,
        const Tank::Physics::TankSettings& settings)
    {
        Tank::Physics::TrackedVehicleTest test;
        test.Initialize(settings, document.environment, document.primitives, document.spawn);
        Tank::Physics::TrackedVehicleTestState state = test.State();
        for (int step = 0; step < options.settleSteps; ++step)
        {
            state = test.Step(options.deltaTimeSeconds);
        }
        const Tank::Physics::Vec3 startPosition = state.bodyPosition;
        Tank::Physics::TankInput input;
        input.throttle = options.throttle;
        input.leftTrack = options.leftTrack;
        input.rightTrack = options.rightTrack;
        test.SetInput(input);
        for (int step = 0; step < options.steps; ++step)
        {
            state = test.Step(options.deltaTimeSeconds);
        }

        const float forwardX = std::sin(document.spawn.yawRadians);
        const float forwardZ = std::cos(document.spawn.yawRadians);
        return {
            state,
            (state.bodyPosition.x - startPosition.x) * forwardX +
                (state.bodyPosition.z - startPosition.z) * forwardZ };
    }

    int RunAllMapsTest(const CliOptions& options)
    {
        if (options.mapDirectory.empty())
        {
            std::cerr << "Maps test requires --map-directory <path>\n";
            return 2;
        }

        const std::filesystem::path directory(options.mapDirectory);
        std::error_code errorCode;
        if (!std::filesystem::is_directory(directory, errorCode))
        {
            std::cerr << "FAIL maps invalid_directory=" << directory.string() << "\n";
            return 1;
        }

        std::vector<std::filesystem::path> mapPaths;
        for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(directory))
        {
            if (entry.is_regular_file() && entry.path().extension() == ".json")
            {
                mapPaths.push_back(entry.path());
            }
        }
        std::sort(mapPaths.begin(), mapPaths.end());
        if (mapPaths.empty())
        {
            std::cerr << "FAIL maps no_json_files\n";
            return 1;
        }

        for (const std::filesystem::path& path : mapPaths)
        {
            Tank::Physics::MapDocument document;
            std::string error;
            if (!LoadMapDocument(path, document, error))
            {
                std::cerr << "FAIL maps file=" << path.filename().string() << " error=" << error << "\n";
                return 1;
            }

            Tank::Physics::TrackedVehicleTest test;
            test.Initialize({}, document.environment, document.primitives, document.spawn);
            const Tank::Physics::TrackedVehicleTestState state = test.Step(options.deltaTimeSeconds);
            if (!std::isfinite(state.bodyPosition.x) || !std::isfinite(state.bodyPosition.y) ||
                !std::isfinite(state.bodyPosition.z))
            {
                std::cerr << "FAIL maps file=" << path.filename().string() << " error=invalid_physics_state\n";
                return 1;
            }
            std::cout << "PASS map-file file=" << path.filename().string()
                      << " name=\"" << document.name << "\" primitives=" << document.primitives.size() << "\n";
        }

        std::cout << "PASS maps count=" << mapPaths.size() << "\n";
        return 0;
    }

    int RunMapTest(const CliOptions& options)
    {
        if (options.mapPath.empty())
        {
            std::cerr << "Map test requires --map <path>\n";
            return 2;
        }
        Tank::Physics::MapDocument document;
        std::string error;
        if (!LoadMapDocument(options.mapPath, document, error))
        {
            std::cerr << "FAIL map parse_error=" << error << "\n";
            return 1;
        }

        Tank::Physics::TankSettings tankSettings;
        if (!LoadTankSettings(options.tankSettingsPath, tankSettings, error))
        {
            std::cerr << "FAIL map tank_settings_error=" << error << "\n";
            return 1;
        }

        const MobilityResult result = EvaluateMobility(options, document, tankSettings);
        const Tank::Physics::TrackedVehicleTestState& state = result.state;
        const bool validState =
            std::isfinite(state.bodyPosition.x) &&
            std::isfinite(state.bodyPosition.y) &&
            std::isfinite(state.bodyPosition.z) &&
            state.stepIndex == options.settleSteps + options.steps;
        if (!validState)
        {
            std::cerr << "FAIL map invalid_physics_state\n";
            return 1;
        }
        if (result.forwardDistance < options.minimumForwardDistance)
        {
            std::cerr << "FAIL map forward_distance=" << result.forwardDistance
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
                  << " forward_distance=" << result.forwardDistance
                  << " final_y=" << state.bodyPosition.y
                  << " max_speed_mps=" << state.maximumSpeedMetersPerSecond
                  << " zero_to_ten_s=" << state.zeroToTenTimeSeconds
                  << " engine_rpm=" << state.engineRpm
                  << " gear=" << state.transmissionGear << "\n";
        return 0;
    }

    int RunMobilitySuite(const CliOptions& options)
    {
        if (options.mapPath.empty() || options.tankSettingsDirectory.empty())
        {
            std::cerr << "Mobility suite requires --map <path> and --tank-settings-directory <path>\n";
            return 2;
        }

        Tank::Physics::MapDocument document;
        std::string error;
        if (!LoadMapDocument(options.mapPath, document, error))
        {
            std::cerr << "FAIL mobility-suite map_error=" << error << "\n";
            return 1;
        }

        const std::filesystem::path directory(options.tankSettingsDirectory);
        std::error_code errorCode;
        if (!std::filesystem::is_directory(directory, errorCode))
        {
            std::cerr << "FAIL mobility-suite invalid_directory=" << directory.string() << "\n";
            return 1;
        }

        std::vector<std::filesystem::path> settingsPaths;
        for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(directory))
        {
            if (entry.is_regular_file() && entry.path().extension() == ".json")
            {
                settingsPaths.push_back(entry.path());
            }
        }
        std::sort(settingsPaths.begin(), settingsPaths.end());
        if (settingsPaths.empty())
        {
            std::cerr << "FAIL mobility-suite no_json_files\n";
            return 1;
        }

        for (const std::filesystem::path& settingsPath : settingsPaths)
        {
            Tank::Physics::TankSettings settings;
            if (!LoadTankSettings(settingsPath.string(), settings, error))
            {
                std::cerr << "FAIL mobility-suite file=" << settingsPath.filename().string()
                          << " error=" << error << "\n";
                return 1;
            }

            const MobilityResult result = EvaluateMobility(options, document, settings);
            const Tank::Physics::TrackedVehicleTestState& state = result.state;
            if (!std::isfinite(result.forwardDistance) || !std::isfinite(state.bodyPosition.y))
            {
                std::cerr << "FAIL mobility-suite file=" << settingsPath.filename().string()
                          << " error=invalid_physics_state\n";
                return 1;
            }

            std::cout << "RESULT mobility-suite file=" << settingsPath.filename().string()
                      << " forward_distance=" << result.forwardDistance
                      << " final_y=" << state.bodyPosition.y
                      << " max_speed_mps=" << state.maximumSpeedMetersPerSecond
                      << " zero_to_ten_s=" << state.zeroToTenTimeSeconds
                      << " engine_rpm=" << state.engineRpm
                      << " gear=" << state.transmissionGear << "\n";
        }

        std::cout << "PASS mobility-suite count=" << settingsPaths.size()
                  << " map=\"" << document.name << "\"\n";
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
    if (options.testName == "maps")
    {
        return RunAllMapsTest(options);
    }
    if (options.testName == "mobility-suite")
    {
        return RunMobilitySuite(options);
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
