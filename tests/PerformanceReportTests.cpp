#include "App/PerformanceReport.h"
#include "Diagnostics/Log.h"
#include <iostream>
#include <stdexcept>

int main()
{
    using namespace Tank::App;
    try
    {
        const auto check = [](bool value, const char* message) { if (!value) throw std::runtime_error(message); };
        PerformanceReportContext context;
        context.mode = "Tracked Vehicle"; context.map = "Test\nStage"; context.paused = "no";
        context.adapter = "Example GPU"; context.renderingPath = "Deferred"; context.renderView = "LightPass";
        context.upscalerEnabled = context.upscalerAvailable = context.upscalerOutputAvailable = true;
        context.upscalerBackend = "Streamline"; context.upscalerQuality = "Quality"; context.vSync = true;
        context.windowWidth = 1920; context.windowHeight = 1080;
        context.viewportX = 240; context.viewportWidth = 1440; context.viewportHeight = 1080;
        context.viewportEnabled = true; context.outputWidth = 1440; context.outputHeight = 1080;
        context.renderWidth = 960; context.renderHeight = 720;
        PerformanceSummary summary; summary.frames = 300; summary.rendererFrames = 300; summary.gpuFrames = 300;
        summary.interval = {17, 20, 22, 100}; summary.host = {15, 16, 18, 30};
        summary.rendererTotal = {14, 15, 17, 25}; summary.gpu = {5, 6, 8, 10};
        summary.renderer[static_cast<size_t>(RtPbrSurvey::RendererCpuPhase::GpuWait)] = {9, 10, 12, 20};
        summary.gpuPasses.push_back({"TemporalUpscalerPass", {4, 4.5f, 7, 9}});
        for (int i = 0; i < 160; ++i) summary.gpuPasses.push_back({"Pass" + std::to_string(i), {0, 0, 0, 0}});
        const std::string report = FormatPerformanceReport(summary, context);
        check(report.find("Mode: Tracked Vehicle; map: Test Stage; paused: no") != std::string::npos,
            "report includes mode and sanitizes multiline names");
        check(report.find("Example GPU") != std::string::npos && report.find("VSync: on") != std::string::npos &&
              report.find("quality: Quality") != std::string::npos && report.find("output used: on") != std::string::npos,
            "report includes hardware and actual renderer/upscaler context");
        check(report.find("Window: 1920x1080") != std::string::npos &&
              report.find("Scene output: 1440x1080; internal render: 960x720") != std::string::npos,
            "window, output and internal sizes are distinguishable");
        check(report.find("FPS from average frame interval: 50.00") != std::string::npos &&
              report.find("GPU fence wait: last=9.00 avg=10.00 p95=12.00 peak=20.00 ms share=66.67%") != std::string::npos &&
              report.find("TemporalUpscalerPass: last=4.00 avg=4.50 p95=7.00 peak=9.00 ms share=75.00%") != std::string::npos,
            "report preserves real metrics and the correct share denominators");
        check(report.size() > 4096 && report.find("Pass159:") != std::string::npos &&
              report.find("=== End Performance Report ===") != std::string::npos,
            "full report includes all passes even when longer than a single log entry");
        Tank::Diagnostics::LogBuffer log;
        size_t lines = 0;
        ForEachPerformanceReportLine(report, [&](const std::string& line)
        {
            check(line.size() < 4096, "individual report lines fit in the log buffer");
            log.Append(Tank::Diagnostics::LogLevel::Info, "Performance", line); ++lines;
        });
        Tank::Diagnostics::LogSnapshot snapshot;
        check(log.ReadIfChanged(snapshot) && snapshot.entries.size() == lines, "Output receives every report line");
        std::string recovered;
        for (const auto& entry : snapshot.entries) { recovered += entry.message; recovered += '\n'; }
        check(recovered == report, "Output receives the exact same complete text as the clipboard report");
        PerformanceSummary unavailable;
        unavailable.frames = 1;
        const auto pending = FormatPerformanceReport(unavailable, context);
        check(pending.find("GPU timing unavailable") != std::string::npos &&
              pending.find("Renderer CPU timing unavailable") != std::string::npos &&
              pending.find("need two frames") != std::string::npos,
            "missing data is explicitly unavailable, not an invented zero measurement");
        unavailable.frames = 0;
        check(FormatPerformanceReport(unavailable, context).find("no host samples yet") != std::string::npos,
            "report handles startup before the first sample");
        context.map = std::string(5000, 'x');
        check(FormatPerformanceReport(unavailable, context).find(std::string(241, 'x')) == std::string::npos,
            "unbounded user names cannot overflow an Output entry");
        std::cout << "PASS PerformanceReport\n";
    }
    catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
