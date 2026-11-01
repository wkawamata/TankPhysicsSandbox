#pragma once

#include "App/PerformanceMeter.h"
#include <iomanip>
#include <locale>
#include <sstream>
#include <string>

namespace Tank::App
{
    struct PerformanceReportContext
    {
        std::string mode = "unknown", map = "n/a", paused = "n/a", build = "unknown";
        std::string adapter = "unknown", renderingPath = "unknown", renderView = "unknown";
        std::string upscalerBackend = "unknown", upscalerQuality = "unknown", upscalerStatus = "unknown";
        bool upscalerEnabled = false, upscalerAvailable = false, upscalerOutputAvailable = false;
        bool hybridReflection = false, shadows = false, viewportEnabled = false, vSync = false;
        int windowWidth = 0, windowHeight = 0, viewportX = 0, viewportY = 0;
        int viewportWidth = 0, viewportHeight = 0, outputWidth = 0, outputHeight = 0;
        int renderWidth = 0, renderHeight = 0;
    };

    inline std::string ReportLabel(std::string value)
    {
        // Keep user-authored names on one line and within the Output log's per-entry limit.
        for (char& c : value) if (c == '\n' || c == '\r' || c == '\0') c = ' ';
        constexpr size_t limit = 240;
        if (value.size() > limit)
        {
            size_t end = limit;
            while (end > 0 && (static_cast<unsigned char>(value[end]) & 0xc0) == 0x80) --end;
            value.resize(end); value += "...";
        }
        return value;
    }

    template<typename Emit>
    void ForEachPerformanceReportLine(const std::string& report, Emit emit)
    {
        size_t begin = 0;
        while (begin < report.size())
        {
            const size_t end = report.find('\n', begin);
            const size_t count = end == std::string::npos ? report.size() - begin : end - begin;
            emit(report.substr(begin, count));
            if (end == std::string::npos) break;
            begin = end + 1;
        }
    }

    inline std::string FormatPerformanceReport(const PerformanceSummary& summary,
                                               const PerformanceReportContext& context)
    {
        std::ostringstream out;
        out.imbue(std::locale::classic());
        out << std::fixed << std::setprecision(2);
        const auto onOff = [](bool v) { return v ? "on" : "off"; };
        const auto stats = [&out](const char* name, const TimingStats& t, float total)
        {
            out << ReportLabel(name) << ": last=" << t.latest << " avg=" << t.average
                << " p95=" << t.p95 << " peak=" << t.peak << " ms";
            if (total > 0) out << " share=" << std::clamp(t.average / total * 100.0f, 0.0f, 100.0f) << "%";
            out << '\n';
        };
        out << "=== Tank Sandbox Performance Report v1 ===\n";
        out << "Mode: " << ReportLabel(context.mode) << "; map: " << ReportLabel(context.map)
            << "; paused: " << ReportLabel(context.paused) << "; build: " << ReportLabel(context.build) << '\n';
        out << "GPU adapter: " << ReportLabel(context.adapter) << '\n';
        out << "Renderer: " << ReportLabel(context.renderingPath) << "; view: " << ReportLabel(context.renderView)
            << "; hybrid reflection: " << onOff(context.hybridReflection) << "; shadows: " << onOff(context.shadows)
            << "; VSync: " << onOff(context.vSync) << '\n';
        out << "DLSS SR: " << onOff(context.upscalerEnabled) << "; backend: " << ReportLabel(context.upscalerBackend)
            << "; quality: " << ReportLabel(context.upscalerQuality) << "; available: " << onOff(context.upscalerAvailable)
            << "; output used: " << onOff(context.upscalerOutputAvailable) << '\n';
        out << "DLSS status: " << ReportLabel(context.upscalerStatus) << '\n';
        out << "Window: " << context.windowWidth << 'x' << context.windowHeight
            << "; custom viewport: " << onOff(context.viewportEnabled) << " (" << context.viewportX << ','
            << context.viewportY << ' ' << context.viewportWidth << 'x' << context.viewportHeight << ")\n";
        out << "Scene output: " << context.outputWidth << 'x' << context.outputHeight
            << "; internal render: " << context.renderWidth << 'x' << context.renderHeight << '\n';
        out << "Samples: host=" << summary.frames << "/300; renderer CPU=" << summary.rendererFrames
            << "; completed GPU=" << summary.gpuFrames << " (GPU readback is delayed)\n";
        if (!summary.frames)
        {
            out << "Timings: unavailable (no host samples yet)\n=== End Performance Report ===\n";
            return out.str();
        }
        out << "FPS from average frame interval: ";
        if (summary.interval.average > 0) out << 1000.0f / summary.interval.average << '\n';
        else out << "unavailable\n";
        if (summary.interval.average > 0)
        {
            stats("Frame interval", summary.interval, 0);
            out << "Outside host tick (approx avg): "
                << std::max(0.0f, summary.interval.average - summary.host.average) << " ms\n";
        }
        else out << "Frame interval / outside host tick: unavailable (need two frames)\n";
        stats("Host elapsed", summary.host, 0);
        out << "-- Host CPU elapsed (share of host; includes waits) --\n";
        for (size_t i = 0; i < summary.cpu.size(); ++i) stats(kCpuPhaseNames[i], summary.cpu[i], summary.host.average);
        out << "-- Renderer CPU breakdown (part of host renderer row) --\n";
        if (summary.rendererFrames)
        {
            stats("Renderer total", summary.rendererTotal, 0);
            for (size_t i = 0; i < summary.renderer.size(); ++i)
                stats(RtPbrSurvey::kRendererCpuPhaseNames[i], summary.renderer[i], summary.rendererTotal.average);
        }
        else out << "Renderer CPU timing unavailable\n";
        out << "-- GPU timings (separate overlapping timeline) --\n";
        if (summary.gpuFrames)
        {
            stats("GPU total", summary.gpu, 0);
            for (const auto& pass : summary.gpuPasses) stats(pass.name.c_str(), pass.time, summary.gpu.average);
        }
        else out << "GPU timing unavailable\n";
        out << "Notes: CPU and GPU overlap; do not add their times. GPU fence wait is CPU waiting; Present may include VSync/driver waits.\n";
        out << "Settings are current; rolling timings may include earlier settings or startup stalls. Collect 300 fresh frames after a change.\n";
        out << "=== End Performance Report ===\n";
        return out.str();
    }
}
