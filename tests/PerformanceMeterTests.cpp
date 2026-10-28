#include "App/PerformanceMeter.h"
#include <iostream>
#include <limits>
#include <stdexcept>

int main()
{
    using namespace Tank::App;
    try
    {
        const auto check = [](bool ok, const char* message) { if (!ok) throw std::runtime_error(message); };
        const auto near = [](float a, float b) { return std::abs(a - b) < .0001f; };
        const std::vector<GpuCheckpoint> checkpoints = {{"StartGpu", 10}, {"Depth", 12},
            {"Lighting", 17}, {"Depth", 18}, {"EndGpu", 20}};
        const auto gpu = ResolveGpuTiming(checkpoints);
        check(gpu.available && near(gpu.totalMs, 10), "GPU total subtracts initial timestamp");
        check(gpu.passes.size() == 3 && near(gpu.passes[0].ms, 3) && near(gpu.passes[1].ms, 5),
            "Cumulative GPU timestamps become disjoint intervals and duplicate names aggregate");
        check(!ResolveGpuTiming({}).available, "Missing queries are unavailable, not zero-cost GPU work");
        const std::vector<GpuCheckpoint> reversed = {{"StartGpu", 3}, {"EndGpu", 2}};
        check(!ResolveGpuTiming(reversed).available, "Reject reversed queries");
        const std::vector<GpuCheckpoint> nonfinite = {{"StartGpu", 0}, {"EndGpu", std::numeric_limits<float>::quiet_NaN()}};
        check(!ResolveGpuTiming(nonfinite).available, "Reject invalid GPU timestamps");
        PerformanceFrame frame;
        frame.Phase(CpuPhase::Input) = 1;
        frame.Phase(CpuPhase::Simulation) = 2;
        frame.Phase(CpuPhase::Renderer) = 4;
        frame.FinishCpu(10);
        check(near(frame.cpu.back(), 3) && near(std::accumulate(frame.cpu.begin(), frame.cpu.end(), 0.0f), 10),
            "Exclusive CPU intervals plus residual exactly cover host time");
        auto invalid = frame;
        invalid.Phase(CpuPhase::Input) = std::numeric_limits<float>::quiet_NaN();
        invalid.Phase(CpuPhase::Simulation) = 20;
        invalid.FinishCpu(5);
        check(near(invalid.cpu[0], 0) && near(invalid.cpu[1], 5) && near(invalid.cpu.back(), 0),
            "Invalid/overlapping measurements cannot inflate CPU total");
        std::vector<float> ordered;
        for (int i=1; i<=100; ++i) ordered.push_back(float(i));
        const auto stats = SummarizeTimings(ordered);
        check(near(stats.average,50.5f) && near(stats.p95,95) && near(stats.peak,100) && near(stats.latest,100),
            "Rolling statistics use nearest-rank p95 and latest insertion");
        PerformanceHistory history;
        for (int i=1; i<=301; ++i)
        {
            PerformanceFrame current; current.intervalMs = float(i); current.FinishCpu(float(i));
            if (i == 301) current.gpu = gpu;
            history.Add(std::move(current));
        }
        history.Refresh();
        check(history.Summary().frames == 300 && near(history.Summary().host.average,151.5f),
            "History drops oldest sample on wraparound");
        check(near(history.Summary().host.latest,301) && near(history.Summary().host.p95,286),
            "Wrapped history preserves chronological order and percentile");
        check(history.Summary().gpuFrames == 1 && near(history.Summary().gpu.average,10),
            "Unavailable GPU samples do not dilute averages");
        PerformanceFrame paused; paused.FinishCpu(2); paused.gpu = ResolveGpuTiming(std::vector<GpuCheckpoint>{{"StartGpu",0},{"EndGpu",2}});
        history.Clear(); history.Add(frame); history.Add(paused); history.Refresh();
        check(near(history.Summary().cpu[static_cast<size_t>(CpuPhase::Simulation)].latest,0),
            "Paused frames record zero simulation instead of stale physics work");
        check(history.Summary().gpuFrames == 1, "Clear removes prior GPU samples");
        history.Add(frame); history.Refresh();
        check(history.Summary().gpuPasses.size() == 1, "Old pass names do not persist after clear");
        history.Clear(); history.Add(PerformanceFrame{.intervalMs=16, .hostMs=8, .gpu=gpu});
        PerformanceFrame tailOnly; tailOnly.gpu = paused.gpu; tailOnly.FinishCpu(2); history.Add(tailOnly); history.Refresh();
        const auto& passes = history.Summary().gpuPasses;
        const auto depth = std::find_if(passes.begin(),passes.end(),[](const auto& p){return p.name=="Depth";});
        check(depth != passes.end() && near(depth->time.average,1.5f), "Absent pass contributes zero on otherwise valid GPU frame");
        check(near(history.Summary().host.average,5) && near(history.Summary().gpu.average,6),
            "CPU and GPU histories remain independent; never sum overlapping timelines");
        history.Clear(); check(history.Summary().frames == 0 && history.Summary().gpuPasses.empty(), "Mode change clears statistics");
        std::cout << "PASS PerformanceMeter\n";
    }
    catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
