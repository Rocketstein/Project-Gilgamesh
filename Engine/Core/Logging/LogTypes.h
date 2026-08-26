#pragma once

#include <chrono>
#include <cstdint>
#include <source_location>
#include <string>
#include <string_view>


enum class LogLevel : uint8_t
{
	Trace,
	Debug,
	Info,
	Warning,
	Error,
	Critical,
	Count
};

enum class LogCategory : uint8_t
{
	Core,
	Platform,
	Runtime,
	Renderer,
	Tools,
	Misc,
	Count,
};

struct LogEntry
{
	LogCategory category = LogCategory::Misc;
	LogLevel level = LogLevel::Info;

	std::string message;
	std::chrono::system_clock::time_point timestamp;
	std::source_location source;
};

constexpr const char* ToString(LogLevel level)
{
	switch (level)
	{
	case LogLevel::Trace:    return "Trace";
	case LogLevel::Debug:    return "Debug";
	case LogLevel::Info:     return "Info";
	case LogLevel::Warning:  return "Warning";
	case LogLevel::Error:    return "Error";
	case LogLevel::Critical: return "Critical";
	default:                 return "Unknown";
	}
}

constexpr const char* ToString(LogCategory category)
{
	switch (category)
	{
	case LogCategory::Core:     return "Core";
	case LogCategory::Platform: return "Platform";
	case LogCategory::Runtime:  return "Runtime";
	case LogCategory::Renderer: return "Renderer";
	case LogCategory::Tools:	return "Tools";
	case LogCategory::Misc:		return "Misc";
	default:                    return "Unknown";
	}
}
