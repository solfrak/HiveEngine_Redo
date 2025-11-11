#include <hive/precomp.h>
#include <hive/core/log.h>
#include <hive/core/assert.h>

#include <fmt/core.h>
#include <array>

namespace hive
{
    const LogCategory LogHiveRoot { "Hive" };

    void LogManager::UnregisterLogger(LoggerId id)
    {
        const auto isLoggerWithId = [id](const auto &loggerPair)
        {
            return loggerPair.first == id;
        };

        auto it = std::find_if(m_Loggers.begin(), m_Loggers.begin() + m_Count, isLoggerWithId);

        const bool isLoggerFound = it != m_Loggers.end();
        if (isLoggerFound)
        {
            *it = m_Loggers[m_Count - 1];
            m_Count--;
        }
    }

    void LogManager::Log(const LogCategory &cat, LogSeverity sev, const char *msg)
    {
        const auto callLoggerFunc = [&](auto &loggerPair)
        {
            loggerPair.second(cat, sev, msg);
        };

        std::for_each(m_Loggers.begin(), m_Loggers.begin() + m_Count, callLoggerFunc);
    }

    ConsoleLogger::ConsoleLogger(LogManager &manager) : m_Manager(manager),
                                                        m_LoggerId(m_Manager.RegisterLogger(this, &ConsoleLogger::Log))
    {
    }

    ConsoleLogger::~ConsoleLogger()
    {
        m_Manager.UnregisterLogger(m_LoggerId);
    }

    void ConsoleLogger::Log(const LogCategory &category, LogSeverity severity, const char *message)
    {
        struct SeverityLabel {
            LogSeverity severity;
            const char* label;
        };

        constexpr std::array<SeverityLabel, 4> severityLabels{{
            {LogSeverity::TRACE, "[TRACE] "},
            {LogSeverity::INFO,  "[INFO] "},
            {LogSeverity::WARN,  "[WARN] "},
            {LogSeverity::ERROR, "[ERROR] "}
        }};

        const char* label{"[UNKNOWN] "};
        for (const auto& sl : severityLabels) {
            if (sl.severity == severity) {
                label = sl.label;
                break;
            }
        }

        fmt::print("{}{} - {}\n", label, category.GetFullPath(), message);
    }
}
