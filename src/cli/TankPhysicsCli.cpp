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
#include <limits>
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
        float throttle = 0.0f;
        float leftTrack = 1.0f;
        float rightTrack = 1.0f;
        float minimumForwardDistance = 0.0f;
        std::optional<float> minimumFinalY;
    };

    bool LoadTankSettings(
        const std::string& path,
        Tank::Physics::TankSettings& settings,
        std::string& error);

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
                  << "  TankPhysicsCli --test mobility --tank-settings Config/Tank/tank_1.json --dt 0.0166667\n"
                  << "  TankPhysicsCli --test map --map Config/Maps/topology_course.json --tank-settings Config/Tank/tank_1.json --settle-steps 180 --steps 300 --throttle 1 --min-forward-distance 5 --min-final-y 0\n";
    }

    float DistanceXZ(const Tank::Physics::Vec3& a, const Tank::Physics::Vec3& b)
    {
        const float x = b.x - a.x;
        const float z = b.z - a.z;
        return std::sqrt(x * x + z * z);
    }

    float YawRadians(const Tank::Physics::Quat& rotation)
    {
        return std::atan2(
            2.0f * (rotation.w * rotation.y + rotation.x * rotation.z),
            1.0f - 2.0f * (rotation.y * rotation.y + rotation.z * rotation.z));
    }

    float WrappedAngleDelta(float from, float to)
    {
        return std::remainder(to - from, 2.0f * 3.14159265358979323846f);
    }

    int RunMobilityTest(const CliOptions& options)
    {
        Tank::Physics::TankSettings settings;
        std::string error;
        if (!LoadTankSettings(options.tankSettingsPath, settings, error))
        {
            std::cerr << "FAIL mobility tank_settings_error=" << error << "\n";
            return 1;
        }

        constexpr int evaluationSteps = 600;
        Tank::Physics::PhysicsEnvironmentSettings environment;
        environment.floorSizeM = 1000.0f;
        Tank::Physics::TrackedVehicleTest straight;
        straight.Initialize(settings, environment);
        Tank::Physics::TrackedVehicleTestState state = straight.State();
        for (int step = 0; step < options.settleSteps; ++step)
        {
            state = straight.Step(options.deltaTimeSeconds);
        }

        Tank::Physics::TankInput drive;
        drive.throttle = 1.0f;
        straight.SetInput(drive);
        for (int step = 0; step < evaluationSteps; ++step)
        {
            state = straight.Step(options.deltaTimeSeconds);
        }
        const float maximumSpeed = state.maximumSpeedMetersPerSecond;
        const float zeroToTenSeconds = state.zeroToTenTimeSeconds;
        const Tank::Physics::Vec3 brakeStart = state.bodyPosition;

        float minimumSuspensionLength = std::numeric_limits<float>::max();
        float maximumSuspensionLength = 0.0f;
        float suspensionImpulseSum = 0.0f;
        float contactAngleSumDegrees = 0.0f;
        int contactCount = 0;
        std::array<float, Tank::Physics::kTankTrackCount> contactSpan = {};
        for (int wheelIndex = 0; wheelIndex < state.wheelCount; ++wheelIndex)
        {
            const Tank::Physics::TrackedWheelState& wheel = state.wheels[wheelIndex];
            minimumSuspensionLength = std::min(minimumSuspensionLength, wheel.suspensionLength);
            maximumSuspensionLength = std::max(maximumSuspensionLength, wheel.suspensionLength);
            if (!wheel.hasContact)
            {
                continue;
            }
            suspensionImpulseSum += wheel.suspensionImpulseNewtonSeconds;
            contactAngleSumDegrees += std::abs(std::asin(std::clamp(
                wheel.contactLongitudinal.y, -1.0f, 1.0f))) *
                (180.0f / 3.14159265358979323846f);
            ++contactCount;
            for (int otherIndex = wheelIndex + 1; otherIndex < state.wheelCount; ++otherIndex)
            {
                const Tank::Physics::TrackedWheelState& other = state.wheels[otherIndex];
                if (other.hasContact && other.trackIndex == wheel.trackIndex)
                {
                    contactSpan[wheel.trackIndex] = std::max(
                        contactSpan[wheel.trackIndex],
                        DistanceXZ(wheel.contactPosition, other.contactPosition));
                }
            }
        }
        const float averageSuspensionImpulse = contactCount > 0
            ? suspensionImpulseSum / static_cast<float>(contactCount)
            : 0.0f;
        const float averageContactAngleDegrees = contactCount > 0
            ? contactAngleSumDegrees / static_cast<float>(contactCount)
            : 0.0f;

        Tank::Physics::TankInput brake;
        brake.brake = true;
        straight.SetInput(brake);
        float brakingTime = 0.0f;
        for (int step = 0; step < evaluationSteps; ++step)
        {
            state = straight.Step(options.deltaTimeSeconds);
            brakingTime += options.deltaTimeSeconds;
            if (state.speedMetersPerSecond < 0.2f)
            {
                break;
            }
        }
        const float brakingDistance = DistanceXZ(brakeStart, state.bodyPosition);
        const bool brakingStopped = state.speedMetersPerSecond < 0.2f;

        Tank::Physics::TrackedVehicleTest turn;
        turn.Initialize(settings, environment);
        Tank::Physics::TrackedVehicleTestState turnState = turn.State();
        for (int step = 0; step < options.settleSteps; ++step)
        {
            turnState = turn.Step(options.deltaTimeSeconds);
        }
        Tank::Physics::TankInput turnInput;
        turnInput.throttle = 1.0f;
        turnInput.leftTrack = 0.0f;
        turnInput.rightTrack = 1.0f;
        turn.SetInput(turnInput);
        float pathLength = 0.0f;
        float yawTravel = 0.0f;
        Tank::Physics::Vec3 previousPosition = turnState.bodyPosition;
        float previousYaw = YawRadians(turnState.bodyRotation);
        for (int step = 0; step < evaluationSteps; ++step)
        {
            turnState = turn.Step(options.deltaTimeSeconds);
            pathLength += DistanceXZ(previousPosition, turnState.bodyPosition);
            const float yaw = YawRadians(turnState.bodyRotation);
            yawTravel += std::abs(WrappedAngleDelta(previousYaw, yaw));
            previousPosition = turnState.bodyPosition;
            previousYaw = yaw;
        }
        const float turningRadius = yawTravel > 0.01f
            ? pathLength / yawTravel
            : std::numeric_limits<float>::infinity();

        const bool passed = std::isfinite(maximumSpeed) && maximumSpeed > 0.0f &&
            std::isfinite(zeroToTenSeconds) && zeroToTenSeconds >= 0.0f &&
            std::isfinite(brakingDistance) && std::isfinite(brakingTime) &&
            std::isfinite(turningRadius) && contactCount > 0;
        std::cout << (passed ? "PASS" : "FAIL") << " mobility"
                  << " max_speed_mps=" << maximumSpeed
                  << " zero_to_ten_s=" << zeroToTenSeconds
                  << " braking_distance_m=" << brakingDistance
                  << " braking_time_s=" << brakingTime
                  << " braking_stopped=" << (brakingStopped ? "true" : "false")
                  << " one_track_radius_m=" << turningRadius
                  << " left_contact_span_m=" << contactSpan[0]
                  << " right_contact_span_m=" << contactSpan[1]
                  << " contact_angle_deg=" << averageContactAngleDegrees
                  << " suspension_min_m=" << minimumSuspensionLength
                  << " suspension_max_m=" << maximumSuspensionLength
                  << " suspension_avg_impulse_ns=" << averageSuspensionImpulse
                  << "\n";
        return passed ? 0 : 1;
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

        Tank::Physics::TrackedVehicleTest test;
        test.Initialize(
            tankSettings,
            document.environment,
            document.primitives,
            document.spawn);
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
                  << " final_y=" << state.bodyPosition.y
                  << " max_speed_mps=" << state.maximumSpeedMetersPerSecond
                  << " zero_to_ten_s=" << state.zeroToTenTimeSeconds
                  << " engine_rpm=" << state.engineRpm
                  << " gear=" << state.transmissionGear << "\n";
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
    if (options.testName == "mobility")
    {
        return RunMobilityTest(options);
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
