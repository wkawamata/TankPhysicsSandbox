#pragma once

#include "Diagnostics/Log.h"

namespace Ui
{
    class OutputPanel
    {
    public:
        bool open = true;
        void Draw();

    private:
        struct Row
        {
            Tank::Diagnostics::LogLevel level;
            std::string text;
        };
        Tank::Diagnostics::LogSnapshot m_snapshot;
        Tank::Diagnostics::LogFilter m_filter;
        std::set<std::string> m_categories;
        std::vector<Row> m_rows;
        size_t m_visibleEntries = 0;
        bool m_autoScroll = true;
        char m_include[256]{};
        char m_exclude[256]{};
        void RebuildRows();
    };
}
