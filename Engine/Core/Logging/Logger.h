#pragma once

#include <cstdint>
#include <filesystem>
#include <format>
#include <memory>
#include <source_location>
#include <string>
#include <type_traits>
#include <utility>

#include "ILogSink.h"
#include "Engine/Core/Text/Utf8.h"

#define GILGAMESH_LOG(Category, Level, ...)          \
    ::Logger::Write(                                 \
        ::LogCategory::Category,                     \
        ::LogLevel::Level,                           \
        std::source_location::current(),             \
        __VA_ARGS__)

class Logger;

namespace LoggerDetail
{
    template<typename T>
    using FormatArgument = std::conditional_t<
        std::is_same_v<
            std::remove_cvref_t<T>,
            std::filesystem::path>,
        std::string,
        T>;

    template<typename T>
    decltype(auto) NormalizeArgument(T&& value)
    {
        if constexpr (std::is_same_v<
            std::remove_cvref_t<T>,
            std::filesystem::path>)
        {
            return Gilgamesh::Text::PathToUtf8(value);
        }
        else
        {
            return std::forward<T>(value);
        }
    }
}

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
        std::format_string<
            LoggerDetail::FormatArgument<Args>...> format,
        Args&&... args)
    {
        if (!ShouldLog(level))
            return;

        LogEntry entry{
            .category = category,
            .level = level,
            .message = std::format(
                format,
                LoggerDetail::NormalizeArgument(
                    std::forward<Args>(args))...),
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
