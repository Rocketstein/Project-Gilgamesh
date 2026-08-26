#include "Logger.h"

#include <algorithm>
#include <atomic>
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

	using SinkList = std::vector<RegisteredSink>;

	struct LoggerState
	{
		LoggerState()
			: sinks(std::make_shared<const SinkList>())
		{
		}

		// Sink-list mutations are rare and serialized here. Published lists are
		// immutable, so logging threads never need this mutex.
		std::mutex mutationMutex;
		std::atomic<std::shared_ptr<const SinkList>> sinks;
		std::uint64_t nextSinkId = 1;

		std::atomic<bool>     hasSinks{ false };
		std::atomic<LogLevel> minimumLevel{ LogLevel::Trace };
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
	std::scoped_lock lock(state.mutationMutex);

	const auto current = state.sinks.load(std::memory_order_acquire);
	const std::uint64_t id = state.nextSinkId;

	auto updated = std::make_shared<SinkList>(*current);
	updated->push_back({ id, std::move(sink) });

	state.sinks.store(
		std::shared_ptr<const SinkList>(std::move(updated)),
		std::memory_order_release);

	state.hasSinks.store(true, std::memory_order_relaxed);

	++state.nextSinkId;              // only after everything that can throw
	return LogSinkRegistration(id);
}

bool Logger::ShouldLog()
{
	const auto sinks = GetLoggerState().sinks.load(
		std::memory_order_acquire);

	return !sinks->empty();
}

void Logger::Dispatch(const LogEntry& entry)
{
	const auto sinks = GetLoggerState().sinks.load(
		std::memory_order_acquire);

	for (const auto& registeredSink : *sinks)
	{
		registeredSink.sink->Write(entry);
	}
}

void Logger::RemoveSink(std::uint64_t id) noexcept
{
	auto& state = GetLoggerState();
	std::scoped_lock lock(state.mutationMutex);

	const auto current = state.sinks.load(std::memory_order_acquire);

	const bool present = std::any_of(
		current->begin(), current->end(),
		[id](const RegisteredSink& s) { return s.id == id; });

	if (!present)
		return;                                  // no allocation at all

	try
	{
		auto updated = std::make_shared<SinkList>();
		updated->reserve(current->size() - 1);

		for (const auto& s : *current)
			if (s.id != id)
				updated->push_back(s);

		state.hasSinks.store(!updated->empty(), std::memory_order_relaxed);

		state.sinks.store(
			std::shared_ptr<const SinkList>(std::move(updated)),
			std::memory_order_release);
	}
	catch (const std::bad_alloc&)
	{
		// Out of memory while tearing down a sink.
	}
}
