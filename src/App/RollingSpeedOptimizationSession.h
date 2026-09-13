#pragma once

#include "Physics/RollingSpeedOptimizer.h"

#include <future>
#include <filesystem>
#include <memory>
#include <optional>

namespace Tank::App
{
    class RollingSpeedOptimizationSession
    {
    public:
        ~RollingSpeedOptimizationSession();
        void Start(const Physics::TankSettings& settings, const Physics::PhysicsEnvironmentSettings& environment);
        void Cancel();
        void Poll();
        bool Running() const { return m_future.valid(); }
        bool Matches(const Physics::TankSettings& settings, const Physics::PhysicsEnvironmentSettings& environment) const;
        bool Apply(Physics::TankSettings& settings, const Physics::PhysicsEnvironmentSettings& environment);
        bool ExportCsv(const std::filesystem::path& directory);
        const std::string& Status() const { return m_status; }
        const std::optional<Physics::RollingSpeedOptimizationResult>& Result() const { return m_result; }
    private:
        struct Progress
        {
            std::atomic<bool> cancelled = false;
            std::atomic<int> evaluations = 0;
            std::atomic<float> rmsMeters = 0;
        };
        std::shared_ptr<Progress> m_progress;
        std::future<Physics::RollingSpeedOptimizationResult> m_future;
        std::optional<Physics::RollingSpeedOptimizationResult> m_result;
        bool m_applyPending = false;
        std::string m_source;
        std::string m_status;
    };
}
