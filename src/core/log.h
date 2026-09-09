// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Zilla contributors.
//
// Minimal logging: everything goes to stdout/stderr, no engine, no singletons.
#ifndef ZILLA_CORE_LOG_H
#define ZILLA_CORE_LOG_H

namespace zilla {

enum class LogLevel {
	Info,
	Warning,
	Error,
};

void log_write(LogLevel p_level, const char *p_format, ...);

[[noreturn]] void fatal(const char *p_format, ...);

} // namespace zilla

#define ZILLA_LOG_INFO(...) zilla::log_write(zilla::LogLevel::Info, __VA_ARGS__)
#define ZILLA_LOG_WARN(...) zilla::log_write(zilla::LogLevel::Warning, __VA_ARGS__)
#define ZILLA_LOG_ERROR(...) zilla::log_write(zilla::LogLevel::Error, __VA_ARGS__)

#endif // ZILLA_CORE_LOG_H
