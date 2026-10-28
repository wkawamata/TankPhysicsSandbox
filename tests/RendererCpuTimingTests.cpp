#include "Runtime/RendererCpuTiming.h"
#include <cmath>
#include <iostream>
#include <numeric>
#include <stdexcept>
#include <type_traits>

int main()
{
    using namespace RtPbrSurvey;
    using Phase = RendererCpuPhase;
    try
    {
        const auto check = [](bool ok, const char* message) { if (!ok) throw std::runtime_error(message); };
        const auto near = [](float a, float b) { return std::abs(a - b) < .0001f; };
        const auto at = [](int ms) { return RendererCpuTimer::TimePoint{} + std::chrono::milliseconds(ms); };
        const auto value = [](const auto& timing, Phase phase) { return timing.ms[static_cast<size_t>(phase)]; };
        RendererCpuTimer timer;
        check(!timer.Completed().available, "No stale timings before first renderer frame");
        timer.Switch(Phase::Present, at(100)); timer.End(at(200));
        check(!timer.Completed().available, "Initialization/shutdown work outside RunFrame is ignored");
        timer.Begin(at(0));
        const auto parent = timer.Switch(Phase::RecordSubmit, at(1));
        const auto recording = timer.Switch(Phase::GpuWait, at(2));
        timer.Switch(recording, at(5)); // Three-ms wait nested in command recording.
        timer.Switch(parent, at(6));
        timer.Switch(Phase::Present, at(7));
        timer.Switch(Phase::Update, at(17));
        timer.End(at(18));
        const auto timing = timer.Completed();
        check(timing.available && near(timing.totalMs,18), "Entire renderer call is covered");
        check(near(value(timing,Phase::Update),3) && near(value(timing,Phase::RecordSubmit),2),
            "Parent phases exclude nested waits and resume afterwards");
        check(near(value(timing,Phase::GpuWait),3) && near(value(timing,Phase::Present),10),
            "Fence waits and Present remain separate");
        check(near(std::accumulate(timing.ms.begin(),timing.ms.end(),0.0f),timing.totalMs),
            "Exclusive phases sum to total without double counting");
        timer.Switch(Phase::GpuWait,at(100)); timer.End(at(200));
        check(near(timer.Completed().totalMs,18), "Out-of-frame waits cannot modify completed snapshot");
        timer.Begin(at(30)); timer.End(at(34));
        check(near(timer.Completed().totalMs,4) && near(value(timer.Completed(),Phase::Present),0),
            "New frame clears old Present/wait samples");
        timer.Begin(at(10)); timer.Switch(Phase::GpuWait,at(9)); timer.End(at(12));
        check(near(timer.Completed().totalMs,2), "Backward test timestamp never creates negative duration");
        static_assert(!std::is_copy_constructible_v<ScopedRendererCpuPhase>);
        timer.Begin();
        {
            ScopedRendererCpuPhase record(timer,Phase::RecordSubmit);
            { ScopedRendererCpuPhase wait(timer,Phase::GpuWait); }
        }
        timer.End();
        check(timer.Completed().available && timer.Completed().totalMs >= 0, "Nested RAII phase scopes close safely");
        std::cout << "PASS RendererCpuTiming\n";
    }
    catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
