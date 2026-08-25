#pragma once

#include <format>
#include <cstdint>
#include <memory>
#include <source_location>
#include <utility>

#include "ILogSink.h"

#define GILGAMESH_LOG(Category, Level, Format, ...)             \
    ::Logger::Write(                                            \
        ::LogCategory::Category,                                \
        ::LogLevel::Level,                                      \
        std::source_location::current(),                        \
        Format __VA_OPT__(,) __VA_ARGS__)

class Logger;

class LogSinkRegistration
{
public:
    LogSinkRegistration() = default;
    ~LogSinkRegistration();

    LogSinkRegistration(const LogSinkRegistration&) = delete;
    LogSinkRegistration& operator=(const LogSinkRegistration&) = delete;

    LogSinkRegistration(LogSinkRegistration&& other) noexcept;
    LogSinkRegistration& operator=(LogSinkRegistration&& other) noexcept;

    void Reset() noexcept;

private:
    friend class Logger;

    explicit LogSinkRegistration(std::uint64_t id) noexcept
        : id_(id)
    {
    }

    std::uint64_t id_ = 0;
};

class Logger
{
public:
	static LogSinkRegistration AddSink(
		std::shared_ptr<ILogSink> sink);

	static bool ShouldLog(LogLevel level);

    template<typename... Args>
    static void Write(
        LogCategory category,
        LogLevel level,
        std::source_location source,
        std::format_string<Args...> format,
        Args&&... args)
    {
        if (!ShouldLog(level))
            return;

        LogEntry entry{
            .category = category,
            .level = level,
            .message = std::format(
                format,
                std::forward<Args>(args)...),
            .timestamp =
                std::chrono::system_clock::now(),
            .source = source
        };

        Dispatch(entry);
    }

private:
    static void Dispatch(const LogEntry& entry);
    static void RemoveSink(std::uint64_t id) noexcept;

    friend class LogSinkRegistration;
};
