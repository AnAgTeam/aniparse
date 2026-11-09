/*
 * Copyright (C) 2025-2026 Toilettrauma
 */
#pragma once
#include <source_location>
#include <string_view>

namespace aniparse {

/// Severity of a message handed to a @ref Logger, ordered from least to most
/// severe.
enum class LogLevel {
	Debug,   ///< Diagnostic detail of interest only when tracing a parse.
	Info,    ///< Ordinary progress of an operation.
	Warning, ///< Something unexpected that the operation recovered from.
	Error,   ///< An operation failed.
	Fatal    ///< A failure the session cannot be expected to continue past.
};

/**
 * @brief Interface for logging messages
 *
 * The sink is shared by every parser of the session (it lives in @ref RuntimeServices),
 * so the emitter's identity travels with each message as @ref log's parser_id rather
 * than being a property of the logger — that is what lets a host filter the stream
 * per parser.
 */
struct Logger {
	virtual ~Logger() = default;

	/**
	 * @brief Output text with prefix and message
	 * @param message_type Prefix to message. ("INFO", "ERROR", etc.)
	 * @param message Message text to output
	 * @param parser_id Source identifier from the context's private binding.
	 *        Empty for an unbound extractor-store context.
	 * @param loc Call site the message originates from, for diagnostics
	 */
	virtual void log(LogLevel message_type,
	                 std::string_view message,
	                 std::string_view parser_id,
	                 const std::source_location loc = std::source_location::current()) = 0;
protected:
	Logger() = default;
	Logger(const Logger&) = default;
	Logger(Logger&&) = default;
	Logger& operator=(const Logger&) = default;
	Logger& operator=(Logger&&) = default;

};

} // namespace aniparse
