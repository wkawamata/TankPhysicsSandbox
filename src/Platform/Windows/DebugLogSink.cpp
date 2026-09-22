#include "stdafx.h"
#include "Platform/Windows/DebugLogSink.h"
#include <Windows.h>

namespace Tank::Platform::Windows
{
    void DebugLogSink(Diagnostics::LogLevel level, std::string_view category, std::string_view message)
    {
        const std::string text = std::string("[") + Diagnostics::LevelName(level) + "][" +
            std::string(category) + "] " + std::string(message) + "\n";
        const int count = MultiByteToWideChar(CP_UTF8, 0, text.data(),
            static_cast<int>(text.size()), nullptr, 0);
        if (count <= 0) return;
        std::wstring wide(static_cast<size_t>(count), L'\0');
        MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), wide.data(), count);
        OutputDebugStringW(wide.c_str());
    }
}
