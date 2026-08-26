#include "Logger.h"

#include <algorithm>
#include <mutex>
#include <utility>
#include <vector>

namespace
{
	struct RegisteredSink
	{
		std::uint64_t id = 0;
		std::shared_ptr<ILogSink> sink;
	};

	struct LoggerState
	{
		std::mutex mutex;
		std::vector<RegisteredSink> sinks;
		std::uint64_t nextSinkId = 1;
	};

	LoggerState& GetLoggerState()
	{
		static LoggerState state;
		return state;
	}
}

LogSinkRegistration::~LogSinkRegistration()
{
	Reset();
}

LogSinkRegistration::LogSinkRegistration(
	LogSinkRegistration&& other) noexcept
	: id_(std::exchange(other.id_, 0))
{
}

LogSinkRegistration& LogSinkRegistration::operator=(
	LogSinkRegistration&& other) noexcept
{
	if (this != &other)
	{
		Reset();
		id_ = std::exchange(other.id_, 0);
	}

	return *this;
}

void LogSinkRegistration::Reset() noexcept
{
	if (id_ == 0)
		return;

	Logger::RemoveSink(id_);
	id_ = 0;
}

LogSinkRegistration Logger::AddSink(
	std::shared_ptr<ILogSink> sink)
{
	if (sink == nullptr)
		return {};

	auto& state = GetLoggerState();
	std::scoped_lock lock(state.mutex);

	const std::uint64_t id = state.nextSinkId++;
	state.sinks.push_back({ id, std::move(sink) });

	return LogSinkRegistration(id);
}

bool Logger::ShouldLog(LogLevel level)
{
	auto& state = GetLoggerState();
	std::scoped_lock lock(state.mutex);

	return !state.sinks.empty();
}

void Logger::Dispatch(const LogEntry& entry)
{
	std::vector<std::shared_ptr<ILogSink>> sinks;

	{
		auto& state = GetLoggerState();
		std::scoped_lock lock(state.mutex);

		sinks.reserve(state.sinks.size());
		for (const auto& registeredSink : state.sinks)
			sinks.push_back(registeredSink.sink);
	}

	for (const auto& sink : sinks)
	{
		sink->Write(entry);
	}
}

void Logger::RemoveSink(std::uint64_t id) noexcept
{
	auto& state = GetLoggerState();
	std::scoped_lock lock(state.mutex);

	std::erase_if(
		state.sinks,
		[id](const RegisteredSink& registeredSink)
		{
			return registeredSink.id == id;
		});
}
