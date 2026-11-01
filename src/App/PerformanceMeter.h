#pragma once

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <numeric>
#include <span>
#include <string>
#include <vector>
#include "Runtime/RendererCpuTiming.h"

namespace Tank::App
{
    using PerformanceClock = std::chrono::steady_clock;
    inline float ElapsedMs(PerformanceClock::time_point start)
    {
        return std::chrono::duration<float, std::milli>(PerformanceClock::now() - start).count();
    }

    enum class CpuPhase { Input, Simulation, SceneSync, Camera, Ui, Renderer, Capture, Other, Count };
    constexpr size_t kCpuPhaseCount = static_cast<size_t>(CpuPhase::Count);
    inline constexpr std::array<const char*, kCpuPhaseCount> kCpuPhaseNames = {
        "Input / gamepad", "Physics / gameplay", "Scene / renderer sync", "Camera / mode",
        "UI / editor", "Renderer / submit / waits", "Capture / logging", "Other host work"
    };
    struct GpuCheckpoint { std::string name; float timeMs = 0; };
    struct GpuPassTime { std::string name; float ms = 0; };
    struct GpuTiming
    {
        bool available = false;
        float totalMs = 0;
        std::vector<GpuPassTime> passes;
    };

    // RtPbr timestamps are cumulative milliseconds, not per-pass durations.
    inline GpuTiming ResolveGpuTiming(std::span<const GpuCheckpoint> checkpoints)
    {
        GpuTiming result;
        if (checkpoints.size() < 2) return result;
        const float start = checkpoints.front().timeMs;
        if (!std::isfinite(start)) return result;
        float previous = start;
        for (size_t i = 1; i < checkpoints.size(); ++i)
        {
            const auto& checkpoint = checkpoints[i];
            if (!std::isfinite(checkpoint.timeMs) || checkpoint.timeMs < previous) return {};
            const float duration = checkpoint.timeMs - previous;
            const auto name = checkpoint.name == "EndGpu" ? "GPU tail" : checkpoint.name;
            const auto found = std::find_if(result.passes.begin(), result.passes.end(),
                [&](const auto& pass) { return pass.name == name; });
            if (found == result.passes.end()) result.passes.push_back({name, duration});
            else found->ms += duration;
            previous = checkpoint.timeMs;
        }
        result.totalMs = previous - start;
        result.available = true;
        return result;
    }

    struct PerformanceFrame
    {
        float intervalMs = 0;
        float hostMs = 0;
        std::array<float, kCpuPhaseCount> cpu = {};
        GpuTiming gpu;
        RtPbrSurvey::RendererCpuTiming renderer;
        float& Phase(CpuPhase phase) { return cpu[static_cast<size_t>(phase)]; }
        void FinishCpu(float totalMs)
        {
            hostMs = std::isfinite(totalMs) ? std::max(0.0f, totalMs) : 0;
            float remaining = hostMs;
            // Exclusive categories form exactly one host interval; never double-count nested work.
            for (size_t i = 0; i + 1 < cpu.size(); ++i)
            {
                cpu[i] = std::isfinite(cpu[i]) ? std::clamp(cpu[i], 0.0f, remaining) : 0;
                remaining -= cpu[i];
            }
            cpu.back() = remaining;
        }
    };

    struct TimingStats
    {
        float latest = 0, average = 0, p95 = 0, peak = 0;
    };
    inline TimingStats SummarizeTimings(std::vector<float> values)
    {
        if (values.empty()) return {};
        TimingStats stats;
        stats.latest = values.back();
        stats.average = std::accumulate(values.begin(), values.end(), 0.0f) / values.size();
        std::sort(values.begin(), values.end());
        stats.p95 = values[static_cast<size_t>(std::ceil(values.size() * .95)) - 1];
        stats.peak = values.back();
        return stats;
    }
    struct GpuPassStats { std::string name; TimingStats time; };
    struct PerformanceSummary
    {
        size_t frames = 0, gpuFrames = 0;
        TimingStats interval, host, gpu;
        size_t rendererFrames = 0;
        TimingStats rendererTotal;
        std::array<TimingStats, RtPbrSurvey::kRendererCpuPhaseCount> renderer;
        std::array<TimingStats, kCpuPhaseCount> cpu;
        std::vector<GpuPassStats> gpuPasses;
    };

    class PerformanceHistory
    {
    public:
        static constexpr size_t kCapacity = 300;
        void Clear() { m_frames = {}; m_count = 0; m_next = 0; m_updates = 0; m_summary = {}; }
        void Add(PerformanceFrame frame)
        {
            m_frames[m_next] = std::move(frame);
            m_next = (m_next + 1) % kCapacity;
            m_count = std::min(m_count + 1, kCapacity);
            // Sorting summaries four times per second at 60 FPS keeps UI overhead modest.
            if (++m_updates % 15 == 0 || m_count == 1) Refresh();
        }
        const PerformanceSummary& Summary() const { return m_summary; }
        void Refresh()
        {
            m_summary = {}; m_summary.frames = m_count;
            std::vector<float> interval, host, gpu, renderer;
            std::array<std::vector<float>, RtPbrSurvey::kRendererCpuPhaseCount> rendererPhases;
            std::array<std::vector<float>, kCpuPhaseCount> phases;
            for (size_t i = 0; i < m_count; ++i)
            {
                const auto& frame = m_frames[(m_next + kCapacity - m_count + i) % kCapacity];
                if (frame.intervalMs > 0 && std::isfinite(frame.intervalMs)) interval.push_back(frame.intervalMs);
                host.push_back(frame.hostMs);
                for (size_t j = 0; j < phases.size(); ++j) phases[j].push_back(frame.cpu[j]);
                if (frame.renderer.available)
                {
                    renderer.push_back(frame.renderer.totalMs);
                    for (size_t j = 0; j < rendererPhases.size(); ++j)
                        rendererPhases[j].push_back(frame.renderer.ms[j]);
                }
                if (!frame.gpu.available) continue;
                gpu.push_back(frame.gpu.totalMs);
                for (const auto& pass : frame.gpu.passes)
                    if (std::none_of(m_summary.gpuPasses.begin(), m_summary.gpuPasses.end(),
                        [&](const auto& value) {return value.name == pass.name;}))
                        m_summary.gpuPasses.push_back({pass.name, {}});
            }
            m_summary.interval = SummarizeTimings(std::move(interval));
            m_summary.host = SummarizeTimings(std::move(host));
            m_summary.rendererFrames = renderer.size();
            m_summary.rendererTotal = SummarizeTimings(std::move(renderer));
            for (size_t j = 0; j < rendererPhases.size(); ++j)
                m_summary.renderer[j] = SummarizeTimings(std::move(rendererPhases[j]));
            m_summary.gpuFrames = gpu.size();
            m_summary.gpu = SummarizeTimings(std::move(gpu));
            for (size_t j = 0; j < phases.size(); ++j) m_summary.cpu[j] = SummarizeTimings(std::move(phases[j]));
            for (auto& pass : m_summary.gpuPasses)
            {
                std::vector<float> values;
                for (size_t i = 0; i < m_count; ++i)
                {
                    const auto& frame = m_frames[(m_next + kCapacity - m_count + i) % kCapacity];
                    if (!frame.gpu.available) continue;
                    const auto found = std::find_if(frame.gpu.passes.begin(), frame.gpu.passes.end(),
                        [&](const auto& value) {return value.name == pass.name;});
                    values.push_back(found == frame.gpu.passes.end() ? 0 : found->ms);
                }
                pass.time = SummarizeTimings(std::move(values));
            }
            std::sort(m_summary.gpuPasses.begin(), m_summary.gpuPasses.end(),
                [](const auto& a, const auto& b) { return a.time.average > b.time.average; });
        }
    private:
        std::array<PerformanceFrame, kCapacity> m_frames;
        size_t m_next = 0, m_count = 0, m_updates = 0;
        PerformanceSummary m_summary;
    };
}
