#pragma once

#include <array>
#include <chrono>
#include <cstdint>
#include <deque>
#include <mutex>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace Tank::Diagnostics
{
    enum class LogLevel { Trace, Debug, Info, Warning, Error, Fatal, Count };
    constexpr size_t kLogLevelCount = static_cast<size_t>(LogLevel::Count);
    const char* LevelName(LogLevel level);

    struct LogEntry
    {
        uint64_t sequence = 0;
        std::chrono::system_clock::time_point time;
        LogLevel level = LogLevel::Info;
        std::string category;
        std::string message;
    };

    struct LogSnapshot
    {
        uint64_t revision = 0;
        uint64_t discarded = 0;
        std::vector<LogEntry> entries;
    };

    // All strings are UTF-8. No dependency on the UI, renderer or platform.
    class LogBuffer
    {
    public:
        explicit LogBuffer(size_t capacity = 10000);
        void Append(LogLevel level, std::string_view category, std::string_view message);
        void Clear();
        bool ReadIfChanged(LogSnapshot& snapshot) const;

    private:
        const size_t m_capacity;
        mutable std::mutex m_mutex;
        std::deque<LogEntry> m_entries;
        uint64_t m_revision = 1;
        uint64_t m_sequence = 0;
        uint64_t m_discarded = 0;
    };

    struct LogFilter
    {
        std::array<bool, kLogLevelCount> levels{true, true, true, true, true, true};
        // Exclusions let newly encountered categories appear automatically.
        std::set<std::string> hiddenCategories;
        std::string includeText;
        std::string excludeText;
        bool Matches(const LogEntry& entry) const;
    };

    using LogSink = void (*)(LogLevel, std::string_view, std::string_view);
    LogBuffer& Logs();
    // Sink must be thread-safe and must not recursively call Write().
    void SetLogSink(LogSink sink);
    void Write(LogLevel level, std::string_view category, std::string_view message);
}
