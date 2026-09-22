#pragma once
#include "Diagnostics/Log.h"

namespace Tank::Platform::Windows
{
    void DebugLogSink(Diagnostics::LogLevel level, std::string_view category, std::string_view message);
}
