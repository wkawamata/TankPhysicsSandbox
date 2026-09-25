#include "Ui/DriveTelemetry.h"

#include <iostream>

namespace
{
    bool Check(bool condition, const char* message)
    {
        if (!condition)
        {
            std::cerr << "FAIL DriveTelemetry: " << message << '\n';
        }
        return condition;
    }
}

int main()
{
    bool passed = true;
    passed &= Check(
        Ui::ClassifyDriveSpeedState(1.0f, 0.0f) == Ui::DriveSpeedState::Acceleration,
        "forward input must be acceleration");
    passed &= Check(
        Ui::ClassifyDriveSpeedState(-1.0f, 0.0f) == Ui::DriveSpeedState::Acceleration,
        "reverse input must be acceleration");
    passed &= Check(
        Ui::ClassifyDriveSpeedState(0.0f, 0.3f) == Ui::DriveSpeedState::NaturalBrake,
        "partial neutral brake must be natural braking");
    passed &= Check(
        Ui::ClassifyDriveSpeedState(1.0f, 1.0f) == Ui::DriveSpeedState::Brake,
        "full brake must override drive input");
    passed &= Check(
        Ui::ClassifyDriveSpeedState(0.0f, 0.0f) == Ui::DriveSpeedState::Coast,
        "no drive or brake input must coast");

    Ui::DriveSpeedHistory history;
    Ui::DriveTelemetryInput telemetry = {};
    telemetry.sample = { 1, 0.1f, 2.0f, Ui::DriveSpeedState::Acceleration };
    history.Update(telemetry);
    telemetry.sample = { 2, 0.2f, 1.0f, Ui::DriveSpeedState::NaturalBrake };
    history.Update(telemetry);
    passed &= Check(history.Samples().size() == 2, "moving samples must be recorded");
    telemetry.stopped = true;
    telemetry.sample = { 3, 0.3f, 0.0f, Ui::DriveSpeedState::NaturalBrake };
    history.Update(telemetry);
    telemetry.sample = { 4, 0.4f, 0.0f, Ui::DriveSpeedState::NaturalBrake };
    history.Update(telemetry);
    passed &= Check(history.Samples().size() == 4,
        "stopped samples must be retained for repeated-input diagnosis");
    telemetry.stopped = false;
    telemetry.sample = { 5, 0.5f, 0.5f, Ui::DriveSpeedState::Acceleration };
    history.Update(telemetry);
    passed &= Check(history.Samples().size() == 5,
        "a new drive must remain in the same diagnostic history");
    telemetry.sample = { 6, 61.0f, 3.0f, Ui::DriveSpeedState::Acceleration };
    history.Update(telemetry);
    passed &= Check(history.Samples().size() == 1,
        "samples older than the 60 second history window must be removed");
    telemetry.sample.stepIndex = 7;
    telemetry.sample.rollPendingSign = -1.0f;
    telemetry.sample.rollPhase = 2;
    telemetry.sample.rollTravelMeters = 3.0f;
    history.Update(telemetry);
    history.Update(telemetry);
    passed &= Check(history.Samples().size() == 2 &&
            history.Samples().back().rollPendingSign == -1.0f &&
            history.Samples().back().rollTravelMeters == 3.0f,
        "rolling samples must retain values without duplicating paused steps");
    telemetry.sample.stepIndex = 0;
    history.Update(telemetry);
    passed &= Check(history.Samples().size() == 1,
        "reset must clear old rolling and driving history");

    if (!passed)
    {
        return 1;
    }
    std::cout << "PASS DriveTelemetry\n";
    return 0;
}
