#pragma once
#include <functional>
namespace Tank::App { class PerformanceHistory; struct PerformanceSummary; }
namespace Ui
{
    void DrawPerformancePanel(Tank::App::PerformanceHistory& history,
        const std::function<void(const Tank::App::PerformanceSummary&)>& dumpReport);
}
