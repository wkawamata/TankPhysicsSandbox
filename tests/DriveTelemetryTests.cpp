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
    history.Update(1, 0.1f, 2.0f, Ui::DriveSpeedState::Acceleration, false);
    history.Update(2, 0.2f, 1.0f, Ui::DriveSpeedState::NaturalBrake, false);
    passed &= Check(history.Samples().size() == 2, "moving samples must be recorded");
    history.Update(3, 0.3f, 0.0f, Ui::DriveSpeedState::NaturalBrake, true);
    history.Update(4, 0.4f, 0.0f, Ui::DriveSpeedState::NaturalBrake, true);
    passed &= Check(history.Samples().size() == 2, "stopped samples must not be recorded");
    history.Update(5, 0.5f, 0.5f, Ui::DriveSpeedState::Acceleration, false);
    passed &= Check(history.Samples().size() == 1, "a new drive must start a fresh history");
    history.Update(6, 61.0f, 3.0f, Ui::DriveSpeedState::Acceleration, false);
    passed &= Check(history.Samples().size() == 1,
        "samples older than the 60 second history window must be removed");

    if (!passed)
    {
        return 1;
    }
    std::cout << "PASS DriveTelemetry\n";
    return 0;
}
