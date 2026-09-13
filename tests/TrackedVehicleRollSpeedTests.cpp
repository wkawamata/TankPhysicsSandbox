#include "Physics/TrackedVehicleTest.h"
#include "Physics/TankSettingsJson.h"
#include "Physics/RollingProfile.h"
#include "Physics/RollingSpeedOptimizer.h"
#include "App/RollingSpeedOptimizationSession.h"

#include <nlohmann/json.hpp>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <limits>
#include <thread>

namespace
{
    bool Check(bool condition, const char* message)
    {
        if (!condition) std::cerr << "FAIL RollSpeed: " << message << '\n';
        return condition;
    }
    std::string Read(const char* name)
    {
        std::ifstream file(std::string(ROLL_SPEED_SOURCE_DIR) + "/tests/fixtures/" + name);
        return std::string(std::istreambuf_iterator<char>(file), {});
    }
    float Distance(Tank::Physics::Vec3 a, Tank::Physics::Vec3 b)
    {
        return std::sqrt((a.x-b.x)*(a.x-b.x)+(a.y-b.y)*(a.y-b.y)+(a.z-b.z)*(a.z-b.z));
    }

    void WriteTrajectoryCsv(const std::filesystem::path& path,
        const Tank::Physics::RollingTrajectory& trajectory, float speed)
    {
        std::ofstream output(path);
        output << "time_seconds,reference_time_seconds,speed_multiplier,move_x_m,move_y_m,move_z_m,"
            "rotation_x,rotation_y,rotation_z,rotation_w,roll_degrees,phase";
        for (int corner = 0; corner < 8; ++corner)
            output << ",corner" << corner << "_x_m,corner" << corner
                << "_y_m,corner" << corner << "_z_m";
        output << '\n' << std::fixed << std::setprecision(7);
        constexpr float dt = 1.0f / 60.0f;
        for (size_t frame = 0; frame < trajectory.samples.size(); ++frame)
        {
            const auto& sample = trajectory.samples[frame];
            const float rollDegrees = 2.0f * std::atan2(sample.rotation.z,
                sample.rotation.w) * 180.0f / 3.14159265358979323846f;
            output << frame * dt << ',' << frame * dt * speed << ',' << speed << ','
                << sample.movement.x << ',' << sample.movement.y << ',' << sample.movement.z << ','
                << sample.rotation.x << ',' << sample.rotation.y << ',' << sample.rotation.z << ','
                << sample.rotation.w << ',' << rollDegrees << ',' << static_cast<int>(sample.phase);
            for (const auto& corner : sample.corners)
                output << ',' << corner.x << ',' << corner.y << ',' << corner.z;
            output << '\n';
        }
    }
}

int main(int argc, char** argv)
{
    using namespace Tank::Physics;
    bool passed = true;
    TankSettings settings;
    std::string error;
    if (!DeserializeTankSettings(Read("rolling_speed_tank_physics_slot1.json"), settings, &error)) return 1;
    RollingProfile profile;
    if (!DeserializeRollingProfile(Read("rolling_speed_rolling_profile_slot1.json"), profile, &error)) return 1;
    ApplyRollingProfile(profile, settings);

    // A known time-compressed path must have zero error. A corner displacement
    // must be detected even when center of mass and event timing are identical.
    RollingTrajectory reference, candidate;
    reference.landingSeconds = 2; reference.finishedSeconds = 4;
    candidate.landingSeconds = 1; candidate.finishedSeconds = 2;
    for (int i = 0; i < 9; ++i)
    {
        RollingTrajectorySample sample;
        for (int c = 0; c < 8; ++c) sample.corners[c] = { static_cast<float>(i), static_cast<float>(c), 0 };
        reference.samples.push_back(sample);
        if (i % 2 == 0) candidate.samples.push_back(sample);
    }
    passed &= Check(CompareRollingTrajectories(reference, candidate).score == 0, "exact x2 trajectory must score zero");
    candidate.samples[2].corners[7].y += 1;
    auto displaced = CompareRollingTrajectories(reference, candidate);
    passed &= Check(displaced.maximumMeters == 1 && displaced.cornerRmsMeters[7] > 0 && displaced.cornerRmsMeters[0] == 0,
        "all eight corners must contribute separately");
    candidate.error = "incomplete";
    passed &= Check(CompareRollingTrajectories(reference, candidate).score >= 1000, "incomplete rolls cannot win");

    RollingSpeedTuning extreme;
    extreme.gravity = std::numeric_limits<float>::quiet_NaN();
    extreme.driveTorque = -4;
    extreme.commitDuration = 100;
    const auto safe = SanitizeRollingSpeedTuning(extreme);
    passed &= Check(std::isfinite(safe.gravity) && safe.driveTorque > 0 && safe.commitDuration <= 2,
        "invalid coefficients must be bounded");
    for (const auto& c : kRollingSpeedCoefficients)
        passed &= Check(RollingSpeedScale(safe.*(c.member), 1) == 1, "x1 coefficients must always be identity");

    std::atomic<bool> cancelled = true;
    const auto cancelledResult = OptimizeRollingSpeed(settings, {}, &cancelled);
    passed &= Check(cancelledResult.cancelled && !cancelledResult.improved && cancelledResult.evaluations == 0,
        "cancellation must stop before simulation");
    auto disabled = settings; disabled.rollingInputEnabled = false;
    passed &= Check(!OptimizeRollingSpeed(disabled).error.empty(), "disabled rolling must fail without applying a result");

    // Run through the same asynchronous session used by the UI button, then
    // verify that Apply installs the x2 result and rejects a stale result.
    Tank::App::RollingSpeedOptimizationSession completedSession;
    completedSession.Start(settings, {});
    while (completedSession.Running())
    {
        completedSession.Poll();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    if (!completedSession.Result()) return 1;
    const auto result = *completedSession.Result();
    auto appliedSettings = settings;
    const float speedBeforeApply = appliedSettings.rollSpeedMultiplier;
    passed &= Check(completedSession.Apply(appliedSettings, {}) &&
            appliedSettings.rollSpeedMultiplier == speedBeforeApply &&
            appliedSettings.rollSpeedTuning == result.tuning &&
            appliedSettings.rollTorqueNm == settings.rollTorqueNm,
        "Optimize must install only fitted internal coefficients");
    passed &= Check(!completedSession.Apply(appliedSettings, {}),
        "an optimized result must be auto-applied only once");
    const std::filesystem::path csvTestDirectory =
        std::filesystem::current_path() / "RollingSpeedCsvTestsTemp";
    std::filesystem::remove_all(csvTestDirectory);
    passed &= Check(completedSession.ExportCsv(csvTestDirectory),
        "completed in-memory measurements must export CSV");
    for (const char* name : {
        "rolling_x0.5.csv", "rolling_x1.0.csv",
        "rolling_x1.5.csv", "rolling_x2.0.csv" })
    {
        std::ifstream csv(csvTestDirectory / name);
        std::string header;
        std::getline(csv, header);
        passed &= Check(header.find("reference_time_seconds") != std::string::npos &&
                header.find("corner7_z_m") != std::string::npos,
            "CSV must contain normalized time, motion and all Local BB corners");
    }
    std::filesystem::remove_all(csvTestDirectory);
    passed &= Check(result.improved && result.error.empty(), "Slot 1 optimization must improve and finish");
    passed &= Check(result.slow.Complete() && result.slowAfter.rmsMeters < 0.2f,
        "Slot 1 x0.5 must match the slowed reference trajectory");
    passed &= Check(result.after.score < result.before.score * 0.1f && result.after.rmsMeters < 0.15f,
        "Slot 1 x2 must closely match the time-compressed corner trajectory");
    passed &= Check(result.after.maximumMeters < 0.5f, "peak corner error must remain bounded");
    passed &= Check(result.after.landingTimeErrorSeconds <= 2.0f / 60.0f,
        "x2 landing must occur at half the x1 time within two physics frames");
    passed &= Check(result.after.finishTimeErrorSeconds < 0.2f, "settling must also run at approximately x2");
    passed &= Check(result.intermediate.Complete() &&
            result.intermediateError.rmsMeters < 0.2f,
        "x1.5 must also match the interpolated reference trajectory");

    // Frozen observations taken before adding internal coefficients.
    const auto golden = nlohmann::json::parse(Read("rolling_speed_x1_bb.json"));
    for (const auto& row : golden)
    {
        const size_t frame = row["frame"].get<size_t>();
        passed &= Check(frame < result.reference.samples.size(), "golden frame must exist");
        if (frame >= result.reference.samples.size()) continue;
        for (int c = 0; c < 8; ++c)
        {
            const auto& v = row["corners"][c];
            passed &= Check(Distance(result.reference.samples[frame].corners[c], {v[0],v[1],v[2]}) < 0.002f,
                "historical x1 Local BB path must be preserved");
        }
    }
    settings.rollSpeedTuning = result.tuning;
    settings.rollSpeedMultiplier = 1;
    const auto unchanged = ObserveRollingTrajectory(settings);
    passed &= Check(unchanged.samples.size() == result.reference.samples.size(), "optimized coefficients must preserve x1 timing");
    for (size_t i = 0; i < (std::min)(unchanged.samples.size(), result.reference.samples.size()); ++i)
        for (int c = 0; c < 8; ++c)
            passed &= Check(Distance(unchanged.samples[i].corners[c], result.reference.samples[i].corners[c]) < 0.00001f,
                "optimized coefficients must preserve every x1 corner");

    const auto leftReference = ObserveRollingTrajectory(settings, {}, -1);
    settings.rollSpeedMultiplier = 2;
    const auto left = ObserveRollingTrajectory(settings, {}, -1);
    const auto leftError = CompareRollingTrajectories(leftReference, left);
    passed &= Check(left.Complete() && leftError.rmsMeters < 0.2f, "fitted coefficients must also work for a left roll");

    if (argc > 2 && std::string(argv[1]) == "--report-dir")
    {
        const std::filesystem::path reportDirectory = argv[2];
        std::filesystem::create_directories(reportDirectory);
        settings.rollSpeedMultiplier = 0.5f;
        const auto x05 = ObserveRollingTrajectory(settings);
        settings.rollSpeedMultiplier = 1.0f;
        const auto x1 = ObserveRollingTrajectory(settings);
        settings.rollSpeedMultiplier = 1.5f;
        const auto x15 = ObserveRollingTrajectory(settings);
        settings.rollSpeedMultiplier = 2.0f;
        const auto x2 = ObserveRollingTrajectory(settings);
        WriteTrajectoryCsv(reportDirectory / "rolling_x0.5.csv", x05, 0.5f);
        WriteTrajectoryCsv(reportDirectory / "rolling_x1.0.csv", x1, 1.0f);
        WriteTrajectoryCsv(reportDirectory / "rolling_x1.5.csv", x15, 1.5f);
        WriteTrajectoryCsv(reportDirectory / "rolling_x2.0.csv", x2, 2.0f);
        std::ofstream summary(reportDirectory / "optimization_summary.txt");
        summary << "x0.5 BB RMS after: " << result.slowAfter.rmsMeters << " m\n"
            << "x2 BB RMS before: " << result.before.rmsMeters << " m\n"
            << "x2 BB RMS after: " << result.after.rmsMeters << " m\n"
            << "x2 BB peak after: " << result.after.maximumMeters << " m\n"
            << "x1 landing: " << result.reference.landingSeconds << " s\n"
            << "x2 landing: " << result.optimized.landingSeconds << " s\n";
    }

    // Exercise the same asynchronous session used by the button: cancellation,
    // source snapshots, and rejection of stale/unavailable results.
    Tank::App::RollingSpeedOptimizationSession session;
    session.Start(settings, {});
    passed &= Check(session.Running() && session.Matches(settings, {}), "button session must capture current settings");
    auto changed = settings; changed.rollTorqueNm += 1000;
    passed &= Check(!session.Matches(changed, {}), "changed torque must invalidate result application");
    changed = settings; changed.chassisWidthM += 0.1f;
    passed &= Check(!session.Matches(changed, {}), "changed geometry must invalidate result application");
    auto environment = PhysicsEnvironmentSettings{}; environment.floorFriction += 0.1f;
    passed &= Check(!session.Matches(settings, environment), "changed floor friction must invalidate result application");
    passed &= Check(!session.Apply(settings, {}), "running session cannot apply a result");
    session.Cancel();
    while (session.Running()) { session.Poll(); std::this_thread::sleep_for(std::chrono::milliseconds(1)); }
    passed &= Check(session.Result() && session.Result()->cancelled && !session.Apply(settings, {}), "cancelled result cannot apply");

    std::cout << "BB RMS " << result.before.rmsMeters << " -> " << result.after.rmsMeters
        << ", x0.5 RMS " << result.slowAfter.rmsMeters
        << ", x1.5 RMS " << result.intermediateError.rmsMeters
        << ", peak " << result.after.maximumMeters << " m; landing target " << result.reference.landingSeconds * 0.5f
        << ", actual " << result.optimized.landingSeconds << " s; finish target " << result.reference.finishedSeconds * 0.5f
        << ", actual " << result.optimized.finishedSeconds << " s; left RMS " << leftError.rmsMeters << '\n';
    if (argc > 1 && std::string(argv[1]) == "--report")
    {
        profile.speedTuning = result.tuning;
        std::cout << SerializeRollingProfile(profile) << '\n';
    }
    std::cout << (passed ? "PASS" : "FAIL") << " Rolling speed optimization\n";
    return passed ? 0 : 1;
}
