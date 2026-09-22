#include "Diagnostics/Log.h"
#include <atomic>
#include <iostream>
#include <thread>

using namespace Tank::Diagnostics;
namespace
{
    bool Check(bool condition, const char* message)
    {
        if (!condition) std::cerr << "FAIL Log: " << message << '\n';
        return condition;
    }
    std::atomic<int> sinkCount{0};
    void Sink(LogLevel, std::string_view, std::string_view) { ++sinkCount; }
}

int main()
{
    bool ok = true;
    LogBuffer buffer(2);
    LogSnapshot snapshot;
    buffer.Append(LogLevel::Info, "Physics", "物理を初期化しました\r\n");
    buffer.ReadIfChanged(snapshot);
    ok &= Check(snapshot.entries.size() == 1 &&
        snapshot.entries[0].message == "物理を初期化しました", "UTF-8 and CRLF normalization");
    ok &= Check(!buffer.ReadIfChanged(snapshot), "unchanged snapshot is not copied");
    LogFilter filter;
    filter.includeText = "物理";
    filter.excludeText = "失敗";
    ok &= Check(filter.Matches(snapshot.entries[0]), "Japanese include/exclude");
    filter.levels[static_cast<size_t>(LogLevel::Info)] = false;
    ok &= Check(!filter.Matches(snapshot.entries[0]), "level filter is ANDed with search");
    filter.levels[static_cast<size_t>(LogLevel::Info)] = true;
    filter.hiddenCategories.insert("Physics");
    ok &= Check(!filter.Matches(snapshot.entries[0]), "category filter");
    filter.hiddenCategories.clear();
    filter.excludeText = "初期化";
    ok &= Check(!filter.Matches(snapshot.entries[0]), "Japanese exclusion wins");
    filter = {};
    ok &= Check(filter.Matches(snapshot.entries[0]), "reset filters");
    buffer.Append(LogLevel::Warning, "Vehicle", "second\nline");
    buffer.Append(LogLevel::Error, "Renderer", "third");
    buffer.ReadIfChanged(snapshot);
    ok &= Check(snapshot.entries.size() == 2 && snapshot.discarded == 1 &&
        snapshot.entries.front().message == "second\nline", "bounded FIFO and multiline preservation");
    buffer.Clear();
    buffer.ReadIfChanged(snapshot);
    ok &= Check(snapshot.entries.empty() && snapshot.discarded == 0, "clear resets contents and dropped count");
    buffer.Append(LogLevel::Info, "", std::string(4095, 'x') + "日本語");
    buffer.ReadIfChanged(snapshot);
    ok &= Check(snapshot.entries[0].message == std::string(4095, 'x') + "..." &&
        snapshot.entries[0].category == "General", "truncation preserves UTF-8 boundaries");
    buffer.Append(LogLevel::Count, "bad", "invalid");
    ok &= Check(!buffer.ReadIfChanged(snapshot), "invalid level is ignored");

    buffer.Append(LogLevel::Info, "bad\ncategory", std::string_view("a\0b", 3));
    buffer.ReadIfChanged(snapshot);
    ok &= Check(snapshot.entries.back().category == "bad category" &&
        snapshot.entries.back().message == "a b", "control characters cannot break rows or hide text");
    for (size_t i = 0; i < kLogLevelCount; ++i)
    {
        LogEntry entry;
        entry.level = static_cast<LogLevel>(i);
        entry.category = "NewCategory";
        filter = {};
        ok &= Check(filter.Matches(entry), "all valid levels and new categories enabled by default");
        filter.levels[i] = false;
        ok &= Check(!filter.Matches(entry), "each level can be hidden independently");
    }
    LogBuffer concurrent(5000);
    std::atomic<int> finished{0};
    std::vector<std::thread> writers;
    for (int i = 0; i < 4; ++i)
        writers.emplace_back([&]() {
            for (int j = 0; j < 1000; ++j) concurrent.Append(LogLevel::Debug, "Worker", "並列ログ");
            ++finished;
        });
    LogSnapshot concurrentSnapshot;
    while (finished.load() != 4) { concurrent.ReadIfChanged(concurrentSnapshot); std::this_thread::yield(); }
    for (auto& writer : writers) writer.join();
    concurrent.ReadIfChanged(concurrentSnapshot);
    ok &= Check(concurrentSnapshot.entries.size() == 4000, "no messages lost with concurrent producers/readers");
    for (size_t i = 1; i < concurrentSnapshot.entries.size(); ++i)
        ok &= Check(concurrentSnapshot.entries[i].sequence == concurrentSnapshot.entries[i - 1].sequence + 1,
            "sequence order is stable");
    SetLogSink(Sink);
    Write(LogLevel::Info, "Test", "sink test");
    SetLogSink(nullptr);
    Write(LogLevel::Info, "Test", "sink detached");
    ok &= Check(sinkCount == 1, "sink attach/detach");
    return ok ? 0 : 1;
}