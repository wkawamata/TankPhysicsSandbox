#include "RollingSpeedOptimizationSession.h"

#include "Physics/RollingProfile.h"
#include "Physics/TankSettingsJson.h"
#include "Physics/PhysicsEnvironmentSettingsJson.h"

#include <chrono>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace Tank::App
{
    namespace
    {
        bool WriteTrajectoryCsv(const std::filesystem::path& path,
            const Physics::RollingTrajectory& trajectory, float speed)
        {
            std::ofstream output(path, std::ios::binary | std::ios::trunc);
            output << "time_seconds,reference_time_seconds,speed_multiplier,move_x_m,move_y_m,move_z_m,"
                "rotation_x,rotation_y,rotation_z,rotation_w,roll_degrees,phase";
            for (int corner = 0; corner < 8; ++corner)
                output << ",corner" << corner << "_x_m,corner" << corner
                    << "_y_m,corner" << corner << "_z_m";
            output << "\r\n" << std::fixed << std::setprecision(7);
            constexpr float dt = 1.0f / 60.0f;
            for (size_t frame = 0; frame < trajectory.samples.size(); ++frame)
            {
                const auto& sample = trajectory.samples[frame];
                const float rollDegrees = 2.0f * std::atan2(
                    sample.rotation.z, sample.rotation.w) *
                    180.0f / 3.14159265358979323846f;
                output << frame * dt << ',' << frame * dt * speed << ',' << speed << ','
                    << sample.movement.x << ',' << sample.movement.y << ',' << sample.movement.z << ','
                    << sample.rotation.x << ',' << sample.rotation.y << ',' << sample.rotation.z << ','
                    << sample.rotation.w << ',' << rollDegrees << ','
                    << static_cast<int>(sample.phase);
                for (const auto& corner : sample.corners)
                    output << ',' << corner.x << ',' << corner.y << ',' << corner.z;
                output << "\r\n";
            }
            return static_cast<bool>(output);
        }

        std::string SourceKey(Physics::TankSettings settings, const Physics::PhysicsEnvironmentSettings& environment)
        {
            settings.rollSpeedMultiplier = 1.0f;
            return Physics::SerializeTankSettings(settings) +
                Physics::SerializeRollingProfile(Physics::ExtractRollingProfile(settings)) +
                Physics::SerializePhysicsEnvironmentSettings(environment);
        }
    }
    RollingSpeedOptimizationSession::~RollingSpeedOptimizationSession()
    {
        Cancel();
        if (m_future.valid()) m_future.wait();
    }
    void RollingSpeedOptimizationSession::Start(const Physics::TankSettings& settings,
        const Physics::PhysicsEnvironmentSettings& environment)
    {
        if (Running()) return;
        m_result.reset();
        m_applyPending = false;
        m_source = SourceKey(settings, environment);
        m_progress = std::make_shared<Progress>();
        m_status = "Recording x1 Local BB trajectory...";
        const auto progress = m_progress;
        m_future = std::async(std::launch::async, [settings, environment, progress]()
        {
            return Physics::OptimizeRollingSpeed(settings, environment, &progress->cancelled,
                [progress](int count, float rms)
                {
                    progress->evaluations.store(count);
                    progress->rmsMeters.store(rms);
                });
        });
    }
    void RollingSpeedOptimizationSession::Cancel()
    {
        if (m_progress) m_progress->cancelled.store(true);
    }
    void RollingSpeedOptimizationSession::Poll()
    {
        if (!Running()) return;
        if (m_future.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
        {
            if (m_progress->cancelled.load()) m_status = "Cancelling...";
            else if (m_progress->evaluations.load() > 0)
            {
                std::ostringstream status;
                status << "Optimizing: " << m_progress->evaluations.load()
                    << " trials / 98, best BB RMS " << std::fixed << std::setprecision(3)
                    << m_progress->rmsMeters.load() << " m";
                m_status = status.str();
            }
            return;
        }
        try { m_result = m_future.get(); }
        catch (const std::exception& error) { m_status = error.what(); return; }
        if (m_result->cancelled) m_status = "Cancelled. Parameters unchanged.";
        else if (!m_result->error.empty()) m_status = m_result->error;
        else m_status = m_result->improved
            ? "x0.5 to x2 optimization complete. Applying internal coefficients..."
            : "No improvement found. Parameters unchanged.";
        m_applyPending = m_result->improved && m_result->error.empty();
    }
    bool RollingSpeedOptimizationSession::Matches(const Physics::TankSettings& settings,
        const Physics::PhysicsEnvironmentSettings& environment) const
    {
        return m_source == SourceKey(settings, environment);
    }
    bool RollingSpeedOptimizationSession::Apply(Physics::TankSettings& settings,
        const Physics::PhysicsEnvironmentSettings& environment)
    {
        if (Running() || !m_applyPending || !m_result || !m_result->improved)
            return false;
        if (!Matches(settings, environment))
        {
            m_applyPending = false;
            m_status = "Settings changed during optimization. Run Optimize again.";
            return false;
        }
        settings.rollSpeedTuning = m_result->tuning;
        m_source = SourceKey(settings, environment);
        m_applyPending = false;
        m_status = "Optimized internal coefficients applied. Choose a speed and Reset Tank.";
        return true;
    }

    bool RollingSpeedOptimizationSession::ExportCsv(
        const std::filesystem::path& directory)
    {
        if (Running() || !m_result || !m_result->improved ||
            !m_result->slow.Complete() ||
            !m_result->reference.Complete() ||
            !m_result->intermediate.Complete() ||
            !m_result->optimized.Complete())
        {
            return false;
        }
        std::error_code error;
        std::filesystem::create_directories(directory, error);
        if (error)
        {
            m_status = "CSV save failed: " + error.message();
            return false;
        }
        if (!WriteTrajectoryCsv(directory / "rolling_x0.5.csv", m_result->slow, 0.5f) ||
            !WriteTrajectoryCsv(directory / "rolling_x1.0.csv", m_result->reference, 1.0f) ||
            !WriteTrajectoryCsv(directory / "rolling_x1.5.csv", m_result->intermediate, 1.5f) ||
            !WriteTrajectoryCsv(directory / "rolling_x2.0.csv", m_result->optimized, 2.0f))
        {
            m_status = "CSV save failed: cannot write report files.";
            return false;
        }
        m_status = "Saved CSV: " + directory.string();
        return true;
    }
}
