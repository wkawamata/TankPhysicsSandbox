#include "Physics/TrackedVehicleTest.h"

#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace
{
    constexpr float kDeltaTimeSeconds = 1.0f / 60.0f;
    constexpr int kSettleSteps = 180;
    constexpr int kTraceSteps = 150;

    struct TraceSample
    {
        Tank::Physics::Vec3 position = {};
        Tank::Physics::Quat rotation = {};
        Tank::Physics::RollingPhase phase =
            Tank::Physics::RollingPhase::None;
    };

    struct RollTrace
    {
        Tank::Physics::TrackedVehicleTest vehicle;
        Tank::Physics::TankInput input = {};
        TraceSample sample = {};
    };

    Tank::Physics::TankSettings MakeSettings()
    {
        Tank::Physics::TankSettings settings;
        settings.rollingInputEnabled = true;
        settings.chassisWidthM = 2.4f;
        settings.chassisLengthM = 3.92f;
        settings.trackSpacingM = 4.63f;
        settings.trackWidthM = 0.63f;
        settings.roadWheelCount = 4;
        settings.rideHeightScale = 1.1f;
        settings.rollTorqueNm = 200000.0f;
        return settings;
    }

    void Initialize(RollTrace& trace, float leverSign)
    {
        trace.vehicle.Initialize(MakeSettings());
        for (int step = 0; step < kSettleSteps; ++step)
        {
            trace.vehicle.Step(kDeltaTimeSeconds);
        }

        trace.input.leftLeverX = leverSign;
        trace.input.rightLeverX = leverSign;
        trace.vehicle.SetInput(trace.input);
        trace.vehicle.Step(kDeltaTimeSeconds);

        trace.input.leftLeverX = 0.0f;
        trace.input.rightLeverX = 0.0f;
        trace.vehicle.SetInput(trace.input);
    }

    TraceSample StepAndCapture(RollTrace& trace)
    {
        trace.vehicle.Step(kDeltaTimeSeconds);
        const auto& state = trace.vehicle.State();
        return { state.bodyPosition, state.bodyRotation, state.rollingPhase };
    }

    bool IsFinite(const TraceSample& sample)
    {
        return std::isfinite(sample.position.x) &&
            std::isfinite(sample.position.y) &&
            std::isfinite(sample.rotation.x) &&
            std::isfinite(sample.rotation.y) &&
            std::isfinite(sample.rotation.z) &&
            std::isfinite(sample.rotation.w);
    }

    void WriteSample(std::ofstream& output, const TraceSample& sample)
    {
        output << sample.position.x << ',' << sample.position.y << ',' <<
            sample.rotation.x << ',' << sample.rotation.y << ',' <<
            sample.rotation.z << ',' << sample.rotation.w << ',' <<
            static_cast<int>(sample.phase);
    }

    bool ParseOptions(
        int argc,
        char** argv,
        std::filesystem::path& outputPath,
        bool& twoRolls)
    {
        for (int index = 1; index < argc; ++index)
        {
            if (std::string(argv[index]) == "--output" && index + 1 < argc)
            {
                outputPath = argv[++index];
            }
            else if (std::string(argv[index]) == "--two-rolls")
            {
                twoRolls = true;
            }
            else
            {
                std::cerr << "Usage: TrackedVehicleRollGifTrace [--output trace.csv] [--two-rolls]\n";
                return false;
            }
        }
        return true;
    }
}

int main(int argc, char** argv)
{
    std::filesystem::path outputPath;
    bool twoRolls = false;
    if (!ParseOptions(argc, argv, outputPath, twoRolls))
    {
        return 2;
    }

    std::ofstream output;
    if (!outputPath.empty())
    {
        std::filesystem::create_directories(outputPath.parent_path());
        output.open(outputPath, std::ios::trunc);
        if (!output)
        {
            std::cerr << "FAIL Roll GIF trace: cannot open output\n";
            return 1;
        }
        output << "frame,negative_x,negative_y,negative_qx,negative_qy,negative_qz,negative_qw,negative_phase,positive_x,positive_y,positive_qx,positive_qy,positive_qz,positive_qw,positive_phase\n";
    }

    RollTrace negative;
    RollTrace positive;
    Initialize(negative, -1.0f);
    Initialize(positive, 1.0f);

    bool passed = true;
    bool secondRollIssued = false;
    int secondRollFrame = -1;
    for (int frame = 0; frame < kTraceSteps; ++frame)
    {
        // The next one-shot lever input is not tied to an arbitrary frame.
        // It is injected only after the first roll landed and made its input
        // latch available again, reproducing two distinct player actions.
        const bool injectSecondRoll = twoRolls &&
            !secondRollIssued &&
            negative.vehicle.State().rollChainAvailable &&
            positive.vehicle.State().rollChainAvailable;
        if (injectSecondRoll)
        {
            negative.input.leftLeverX = -1.0f;
            negative.input.rightLeverX = -1.0f;
            negative.vehicle.SetInput(negative.input);
            positive.input.leftLeverX = 1.0f;
            positive.input.rightLeverX = 1.0f;
            positive.vehicle.SetInput(positive.input);
            secondRollIssued = true;
            secondRollFrame = frame;
        }
        negative.sample = StepAndCapture(negative);
        positive.sample = StepAndCapture(positive);
        if (injectSecondRoll)
        {
            negative.input.leftLeverX = 0.0f;
            negative.input.rightLeverX = 0.0f;
            negative.vehicle.SetInput(negative.input);
            positive.input.leftLeverX = 0.0f;
            positive.input.rightLeverX = 0.0f;
            positive.vehicle.SetInput(positive.input);
        }
        passed &= IsFinite(negative.sample) && IsFinite(positive.sample);
        if (output)
        {
            output << frame << ',';
            WriteSample(output, negative.sample);
            output << ',';
            WriteSample(output, positive.sample);
            output << '\n';
        }
    }

    if (!passed)
    {
        std::cerr << "FAIL Roll GIF trace: non-finite physics sample\n";
        return 1;
    }

    std::cout << "PASS Roll GIF trace frames=" << kTraceSteps
        << " two_rolls=" << (twoRolls ? "true" : "false");
    if (twoRolls)
    {
        std::cout << " second_roll_frame=" << secondRollFrame;
    }
    if (!outputPath.empty())
    {
        std::cout << " output=" << outputPath.string();
    }
    std::cout << '\n';
    return 0;
}
