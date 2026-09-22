#include "Diagnostics/Log.h"

#include <algorithm>
#include <atomic>

namespace Tank::Diagnostics
{
    namespace
    {
        std::atomic<LogSink> s_sink{nullptr};

        std::string BoundedUtf8(std::string_view text, size_t limit)
        {
            if (text.size() <= limit) return std::string(text);
            size_t end = limit;
            while (end > 0 && (static_cast<unsigned char>(text[end]) & 0xc0) == 0x80)
                --end;
            return std::string(text.substr(0, end)) + "...";
        }
    }

    const char* LevelName(LogLevel level)
    {
        constexpr const char* names[]{"Trace", "Debug", "Info", "Warning", "Error", "Fatal"};
        const auto index = static_cast<size_t>(level);
        return index < kLogLevelCount ? names[index] : "Unknown";
    }

    LogBuffer::LogBuffer(size_t capacity) : m_capacity((std::max)(size_t{1}, capacity)) {}

    void LogBuffer::Append(LogLevel level, std::string_view category, std::string_view message)
    {
        if (static_cast<size_t>(level) >= kLogLevelCount) return;
        LogEntry entry;
        entry.level = level;
        entry.category = BoundedUtf8(category.empty() ? "General" : category, 128);
        entry.message = BoundedUtf8(message, 4096);
        for (char& c : entry.category)
            if (c == '\r' || c == '\n' || c == '\0') c = ' ';
        std::replace(entry.message.begin(), entry.message.end(), '\0', ' ');
        // Normalize line endings for ImGui and clipboard output.
        std::erase(entry.message, '\r');
        while (!entry.message.empty() && entry.message.back() == '\n') entry.message.pop_back();
        std::lock_guard lock(m_mutex);
        entry.sequence = ++m_sequence;
        entry.time = std::chrono::system_clock::now();
        if (m_entries.size() == m_capacity)
        {
            m_entries.pop_front();
            ++m_discarded;
        }
        m_entries.push_back(std::move(entry));
        ++m_revision;
    }

    void LogBuffer::Clear()
    {
        std::lock_guard lock(m_mutex);
        m_entries.clear();
        m_discarded = 0;
        ++m_revision;
    }

    bool LogBuffer::ReadIfChanged(LogSnapshot& snapshot) const
    {
        std::lock_guard lock(m_mutex);
        if (snapshot.revision == m_revision) return false;
        snapshot.entries.assign(m_entries.begin(), m_entries.end());
        snapshot.revision = m_revision;
        snapshot.discarded = m_discarded;
        return true;
    }

    bool LogFilter::Matches(const LogEntry& entry) const
    {
        const auto index = static_cast<size_t>(entry.level);
        return index < levels.size() && levels[index] &&
            !hiddenCategories.contains(entry.category) &&
            (includeText.empty() || entry.message.find(includeText) != std::string::npos) &&
            (excludeText.empty() || entry.message.find(excludeText) == std::string::npos);
    }

    LogBuffer& Logs()
    {
        static LogBuffer buffer;
        return buffer;
    }

    void SetLogSink(LogSink sink) { s_sink.store(sink); }

    void Write(LogLevel level, std::string_view category, std::string_view message)
    {
        if (static_cast<size_t>(level) >= kLogLevelCount) return;
        Logs().Append(level, category, message);
        if (const auto sink = s_sink.load()) sink(level, category, message);
    }
}
