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
    int RunMobilityTest(const CliOptions& options);

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
                  << "  TankPhysicsCli --test mobility-slope --tank-settings Config/Tank/tank_1.json --dt 0.0166667\n"
                  << "  TankPhysicsCli --test mobility-step --tank-settings Config/Tank/tank_1.json --dt 0.0166667\n"
                  << "  TankPhysicsCli --test mobility-orientation --tank-settings Config/Tank/tank_1.json --dt 0.0166667\n"
                  << "  TankPhysicsCli --test mobility-all --tank-settings Config/Tank/tank_1.json --dt 0.0166667\n"
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

    struct TurnEvaluation
    {
        float radiusMeters = 0.0f;
        float pathLengthMeters = 0.0f;
        float yawDegrees = 0.0f;
        float maximumYawSpeedDegrees = 0.0f;
    };

    struct SlopeEvaluation
    {
        float holdSlideMeters = 0.0f;
        float lateralSlideMeters = 0.0f;
    };

    struct StepEvaluation
    {
        float heightMeters = 0.0f;
        bool succeeded = false;
        float traversalTimeSeconds = -1.0f;
        float maximumBodyHeightMeters = 0.0f;
        float finalForwardMeters = 0.0f;
    };

    struct OrientationEvaluation
    {
        float forwardDistanceMeters = 0.0f;
        float maximumSpeedMetersPerSecond = 0.0f;
        float zeroToTenSeconds = -1.0f;
        float pivotYawDegrees = 0.0f;
        int lowerContactCount = 0;
        int upperContactCount = 0;
    };

    TurnEvaluation EvaluateTurn(
        const Tank::Physics::TankSettings& settings,
        const Tank::Physics::PhysicsEnvironmentSettings& environment,
        float deltaTimeSeconds,
        int settleSteps,
        float leftTrack,
        float rightTrack)
    {
        constexpr int evaluationSteps = 600;
        Tank::Physics::TrackedVehicleTest test;
        test.Initialize(settings, environment);
        Tank::Physics::TrackedVehicleTestState state = test.State();
        for (int step = 0; step < settleSteps; ++step)
        {
            state = test.Step(deltaTimeSeconds);
        }

        Tank::Physics::TankInput input;
        input.throttle = 1.0f;
        input.leftTrack = leftTrack;
        input.rightTrack = rightTrack;
        test.SetInput(input);

        TurnEvaluation result;
        Tank::Physics::Vec3 previousPosition = state.bodyPosition;
        float previousYaw = YawRadians(state.bodyRotation);
        float yawTravel = 0.0f;
        for (int step = 0; step < evaluationSteps; ++step)
        {
            state = test.Step(deltaTimeSeconds);
            result.pathLengthMeters += DistanceXZ(previousPosition, state.bodyPosition);
            const float yaw = YawRadians(state.bodyRotation);
            yawTravel += std::abs(WrappedAngleDelta(previousYaw, yaw));
            result.maximumYawSpeedDegrees = std::max(
                result.maximumYawSpeedDegrees,
                std::abs(state.yawSpeedDegrees));
            previousPosition = state.bodyPosition;
            previousYaw = yaw;
        }
        result.yawDegrees = yawTravel * (180.0f / 3.14159265358979323846f);
        result.radiusMeters = yawTravel > 0.01f
            ? result.pathLengthMeters / yawTravel
            : std::numeric_limits<float>::infinity();
        return result;
    }

    float EvaluateSlopeSlide(
        const Tank::Physics::TankSettings& sourceSettings,
        float deltaTimeSeconds,
        float friction,
        float tankYawRadians,
        float rampHeightMeters,
        bool fullBrake)
    {
        Tank::Physics::TankSettings settings = sourceSettings;
        Tank::Physics::PhysicsEnvironmentSettings environment;
        environment.floorSizeM = 200.0f;
        environment.obstacleCount = 0;

        Tank::Physics::MapPrimitive ramp;
        ramp.type = Tank::Physics::MapPrimitiveType::TriangularPrism;
        ramp.position = { 0.0f, 0.5f * rampHeightMeters, 0.0f };
        ramp.size = { 20.0f, rampHeightMeters, 24.0f };
        ramp.friction = friction;
        Tank::Physics::MapSpawn spawn;
        spawn.position = { 0.0f, 0.5f * rampHeightMeters + 2.5f, 0.0f };
        spawn.yawRadians = tankYawRadians;

        Tank::Physics::TrackedVehicleTest test;
        test.Initialize(settings, environment, { ramp }, spawn);
        if (fullBrake)
        {
            Tank::Physics::TankInput brake;
            brake.brake = true;
            test.SetInput(brake);
        }
        Tank::Physics::TrackedVehicleTestState state = test.State();
        for (int step = 0; step < 120; ++step)
        {
            state = test.Step(deltaTimeSeconds);
        }
        const Tank::Physics::Vec3 start = state.bodyPosition;
        for (int step = 0; step < 300; ++step)
        {
            state = test.Step(deltaTimeSeconds);
        }
        return std::abs(state.bodyPosition.z - start.z);
    }

    SlopeEvaluation EvaluateSlope(
        const Tank::Physics::TankSettings& settings,
        float deltaTimeSeconds,
        float friction)
    {
        SlopeEvaluation result;
        result.holdSlideMeters = EvaluateSlopeSlide(
            settings, deltaTimeSeconds, friction, 0.0f, 9.0f, true);
        result.lateralSlideMeters = EvaluateSlopeSlide(
            settings,
            deltaTimeSeconds,
            friction,
            0.5f * 3.14159265358979323846f,
            18.0f,
            false);
        return result;
    }

    int RunSlopeTest(const CliOptions& options)
    {
        Tank::Physics::TankSettings settings;
        std::string error;
        if (!LoadTankSettings(options.tankSettingsPath, settings, error))
        {
            std::cerr << "FAIL mobility-slope tank_settings_error=" << error << "\n";
            return 1;
        }

        const SlopeEvaluation low = EvaluateSlope(settings, options.deltaTimeSeconds, 0.3f);
        const SlopeEvaluation medium = EvaluateSlope(settings, options.deltaTimeSeconds, 0.6f);
        const SlopeEvaluation high = EvaluateSlope(settings, options.deltaTimeSeconds, 1.0f);
        const bool passed =
            std::isfinite(low.holdSlideMeters) && std::isfinite(low.lateralSlideMeters) &&
            std::isfinite(medium.holdSlideMeters) && std::isfinite(medium.lateralSlideMeters) &&
            std::isfinite(high.holdSlideMeters) && std::isfinite(high.lateralSlideMeters) &&
            low.holdSlideMeters < 0.5f && medium.holdSlideMeters < 0.5f &&
            high.holdSlideMeters < 0.5f &&
            high.lateralSlideMeters <= low.lateralSlideMeters;
        std::cout << (passed ? "PASS" : "FAIL") << " mobility-slope"
                  << " low_hold_slide_m=" << low.holdSlideMeters
                  << " low_lateral_slide_m=" << low.lateralSlideMeters
                  << " medium_hold_slide_m=" << medium.holdSlideMeters
                  << " medium_lateral_slide_m=" << medium.lateralSlideMeters
                  << " high_hold_slide_m=" << high.holdSlideMeters
                  << " high_lateral_slide_m=" << high.lateralSlideMeters
                  << "\n";
        return passed ? 0 : 1;
    }

    StepEvaluation EvaluateStep(
        const Tank::Physics::TankSettings& settings,
        float deltaTimeSeconds,
        float heightMeters)
    {
        Tank::Physics::PhysicsEnvironmentSettings environment;
        environment.obstacleCount = 0;
        Tank::Physics::MapPrimitive stepBlock;
        stepBlock.type = Tank::Physics::MapPrimitiveType::Box;
        stepBlock.position = { 0.0f, 0.5f * heightMeters, 10.0f };
        stepBlock.size = { 20.0f, heightMeters, 4.0f };
        stepBlock.friction = 1.0f;

        Tank::Physics::TrackedVehicleTest test;
        test.Initialize(settings, environment, { stepBlock });
        Tank::Physics::TrackedVehicleTestState state = test.State();
        for (int step = 0; step < 180; ++step)
        {
            state = test.Step(deltaTimeSeconds);
        }
        const Tank::Physics::Vec3 start = state.bodyPosition;
        StepEvaluation result;
        result.heightMeters = heightMeters;
        result.maximumBodyHeightMeters = state.bodyPosition.y;

        Tank::Physics::TankInput input;
        input.throttle = 1.0f;
        test.SetInput(input);
        for (int step = 0; step < 480; ++step)
        {
            state = test.Step(deltaTimeSeconds);
            result.maximumBodyHeightMeters = std::max(
                result.maximumBodyHeightMeters,
                state.bodyPosition.y);
            result.finalForwardMeters = state.bodyPosition.z - start.z;
            if (state.bodyPosition.z >= 14.0f)
            {
                result.succeeded = true;
                result.traversalTimeSeconds = static_cast<float>(step + 1) * deltaTimeSeconds;
                break;
            }
        }
        return result;
    }

    int RunStepTest(const CliOptions& options)
    {
        Tank::Physics::TankSettings settings;
        std::string error;
        if (!LoadTankSettings(options.tankSettingsPath, settings, error))
        {
            std::cerr << "FAIL mobility-step tank_settings_error=" << error << "\n";
            return 1;
        }

        constexpr std::array<float, 6> heights = {
            0.25f, 0.50f, 0.75f, 1.00f, 1.25f, 1.50f };
        float maximumSuccessfulHeight = 0.0f;
        float maximumHeightTime = -1.0f;
        bool valid = true;
        std::cout << "step-results";
        for (float height : heights)
        {
            const StepEvaluation result = EvaluateStep(
                settings, options.deltaTimeSeconds, height);
            valid &= std::isfinite(result.maximumBodyHeightMeters) &&
                std::isfinite(result.finalForwardMeters);
            if (result.succeeded)
            {
                maximumSuccessfulHeight = height;
                maximumHeightTime = result.traversalTimeSeconds;
            }
            std::cout << " h" << static_cast<int>(height * 100.0f)
                      << "=" << (result.succeeded ? "pass" : "fail")
                      << ":" << result.traversalTimeSeconds;
        }
        std::cout << "\n";
        const bool passed = valid && maximumSuccessfulHeight >= 0.5f;
        std::cout << (passed ? "PASS" : "FAIL") << " mobility-step"
                  << " maximum_height_m=" << maximumSuccessfulHeight
                  << " traversal_time_s=" << maximumHeightTime
                  << "\n";
        return passed ? 0 : 1;
    }

    OrientationEvaluation EvaluateOrientation(
        const Tank::Physics::TankSettings& sourceSettings,
        float deltaTimeSeconds,
        bool inverted)
    {
        Tank::Physics::TankSettings settings = sourceSettings;
        settings.startUpsideDown = inverted;
        Tank::Physics::PhysicsEnvironmentSettings environment;
        environment.floorSizeM = 1000.0f;
        environment.obstacleCount = 0;

        Tank::Physics::TrackedVehicleTest straight;
        straight.Initialize(settings, environment);
        Tank::Physics::TrackedVehicleTestState state = straight.State();
        for (int step = 0; step < 180; ++step)
        {
            state = straight.Step(deltaTimeSeconds);
        }
        OrientationEvaluation result;
        for (int wheelIndex = 0; wheelIndex < state.wheelCount; ++wheelIndex)
        {
            const Tank::Physics::TrackedWheelState& wheel = state.wheels[wheelIndex];
            result.lowerContactCount += !wheel.upperSurface && wheel.hasContact ? 1 : 0;
            result.upperContactCount += wheel.upperSurface && wheel.hasContact ? 1 : 0;
        }
        const Tank::Physics::Vec3 start = state.bodyPosition;
        Tank::Physics::TankInput forward;
        forward.throttle = 1.0f;
        straight.SetInput(forward);
        for (int step = 0; step < 300; ++step)
        {
            state = straight.Step(deltaTimeSeconds);
        }
        result.forwardDistanceMeters = state.bodyPosition.z - start.z;
        result.maximumSpeedMetersPerSecond = state.maximumSpeedMetersPerSecond;
        result.zeroToTenSeconds = state.zeroToTenTimeSeconds;

        Tank::Physics::TrackedVehicleTest pivot;
        pivot.Initialize(settings, environment);
        state = pivot.State();
        for (int step = 0; step < 180; ++step)
        {
            state = pivot.Step(deltaTimeSeconds);
        }
        float previousYaw = YawRadians(state.bodyRotation);
        float yawTravel = 0.0f;
        Tank::Physics::TankInput pivotInput;
        pivotInput.throttle = 1.0f;
        pivotInput.leftTrack = -1.0f;
        pivotInput.rightTrack = 1.0f;
        pivot.SetInput(pivotInput);
        for (int step = 0; step < 180; ++step)
        {
            state = pivot.Step(deltaTimeSeconds);
            const float yaw = YawRadians(state.bodyRotation);
            yawTravel += std::abs(WrappedAngleDelta(previousYaw, yaw));
            previousYaw = yaw;
        }
        result.pivotYawDegrees = yawTravel * (180.0f / 3.14159265358979323846f);
        return result;
    }

    int RunOrientationTest(const CliOptions& options)
    {
        Tank::Physics::TankSettings settings;
        std::string error;
        if (!LoadTankSettings(options.tankSettingsPath, settings, error))
        {
            std::cerr << "FAIL mobility-orientation tank_settings_error=" << error << "\n";
            return 1;
        }
        const OrientationEvaluation upright = EvaluateOrientation(
            settings, options.deltaTimeSeconds, false);
        const OrientationEvaluation inverted = EvaluateOrientation(
            settings, options.deltaTimeSeconds, true);
        const float distanceRatio = upright.forwardDistanceMeters > 0.0f
            ? inverted.forwardDistanceMeters / upright.forwardDistanceMeters
            : 0.0f;
        const float pivotRatio = upright.pivotYawDegrees > 0.0f
            ? inverted.pivotYawDegrees / upright.pivotYawDegrees
            : 0.0f;
        const bool passed =
            std::isfinite(upright.forwardDistanceMeters) &&
            std::isfinite(inverted.forwardDistanceMeters) &&
            std::isfinite(upright.pivotYawDegrees) &&
            std::isfinite(inverted.pivotYawDegrees) &&
            upright.lowerContactCount > 0 && inverted.upperContactCount > 0 &&
            upright.forwardDistanceMeters > 1.0f && inverted.forwardDistanceMeters > 1.0f &&
            upright.pivotYawDegrees > 10.0f && inverted.pivotYawDegrees > 10.0f &&
            distanceRatio >= 0.9f && distanceRatio <= 1.1f &&
            pivotRatio >= 0.9f && pivotRatio <= 1.1f;
        std::cout << (passed ? "PASS" : "FAIL") << " mobility-orientation"
                  << " upright_distance_m=" << upright.forwardDistanceMeters
                  << " inverted_distance_m=" << inverted.forwardDistanceMeters
                  << " distance_ratio=" << distanceRatio
                  << " upright_max_speed_mps=" << upright.maximumSpeedMetersPerSecond
                  << " inverted_max_speed_mps=" << inverted.maximumSpeedMetersPerSecond
                  << " upright_zero_to_ten_s=" << upright.zeroToTenSeconds
                  << " inverted_zero_to_ten_s=" << inverted.zeroToTenSeconds
                  << " upright_pivot_yaw_deg=" << upright.pivotYawDegrees
                  << " inverted_pivot_yaw_deg=" << inverted.pivotYawDegrees
                  << " pivot_ratio=" << pivotRatio
                  << " upright_lower_contacts=" << upright.lowerContactCount
                  << " inverted_upper_contacts=" << inverted.upperContactCount
                  << "\n";
        return passed ? 0 : 1;
    }

    int RunAllMobilityTests(const CliOptions& options)
    {
        const int mobility = RunMobilityTest(options);
        const int slope = RunSlopeTest(options);
        const int step = RunStepTest(options);
        const int orientation = RunOrientationTest(options);
        const bool passed =
            mobility == 0 && slope == 0 && step == 0 && orientation == 0;
        std::cout << (passed ? "PASS" : "FAIL") << " mobility-all"
                  << " mobility=" << mobility
                  << " slope=" << slope
                  << " step=" << step
                  << " orientation=" << orientation
                  << "\n";
        return passed ? 0 : 1;
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

        const TurnEvaluation normalTurn = EvaluateTurn(
            settings, environment, options.deltaTimeSeconds, options.settleSteps, 0.6f, 1.0f);
        const TurnEvaluation oneTrackTurn = EvaluateTurn(
            settings, environment, options.deltaTimeSeconds, options.settleSteps, 0.0f, 1.0f);
        const TurnEvaluation pivotTurn = EvaluateTurn(
            settings, environment, options.deltaTimeSeconds, options.settleSteps, -1.0f, 1.0f);

        const bool passed = std::isfinite(maximumSpeed) && maximumSpeed > 0.0f &&
            std::isfinite(zeroToTenSeconds) && zeroToTenSeconds >= 0.0f &&
            std::isfinite(brakingDistance) && std::isfinite(brakingTime) &&
            std::isfinite(normalTurn.radiusMeters) &&
            std::isfinite(oneTrackTurn.radiusMeters) &&
            std::isfinite(pivotTurn.radiusMeters) && contactCount > 0;
        std::cout << (passed ? "PASS" : "FAIL") << " mobility"
                  << " max_speed_mps=" << maximumSpeed
                  << " zero_to_ten_s=" << zeroToTenSeconds
                  << " braking_distance_m=" << brakingDistance
                  << " braking_time_s=" << brakingTime
                  << " braking_stopped=" << (brakingStopped ? "true" : "false")
                  << " normal_turn_radius_m=" << normalTurn.radiusMeters
                  << " normal_turn_yaw_deg=" << normalTurn.yawDegrees
                  << " one_track_radius_m=" << oneTrackTurn.radiusMeters
                  << " one_track_yaw_deg=" << oneTrackTurn.yawDegrees
                  << " pivot_radius_m=" << pivotTurn.radiusMeters
                  << " pivot_yaw_deg=" << pivotTurn.yawDegrees
                  << " pivot_max_yaw_speed_deg_s=" << pivotTurn.maximumYawSpeedDegrees
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
    if (options.testName == "mobility-slope")
    {
        return RunSlopeTest(options);
    }
    if (options.testName == "mobility-step")
    {
        return RunStepTest(options);
    }
    if (options.testName == "mobility-orientation")
    {
        return RunOrientationTest(options);
    }
    if (options.testName == "mobility-all")
    {
        return RunAllMobilityTests(options);
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
