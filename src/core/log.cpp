// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Zilla contributors.

#include "core/log.h"

#include <cstdarg>
#include <cstdio>
#include <cstdlib>

namespace zilla {

void log_write(LogLevel p_level, const char *p_format, ...) {
	std::va_list args;
	va_start(args, p_format);

	switch (p_level) {
		case LogLevel::Info:
			std::fputs("[zilla] ", stdout);
			std::vprintf(p_format, args);
			break;
		case LogLevel::Warning:
			std::fputs("[zilla][warn] ", stderr);
			std::vfprintf(stderr, p_format, args);
			break;
		case LogLevel::Error:
			std::fputs("[zilla][error] ", stderr);
			std::vfprintf(stderr, p_format, args);
			break;
	}
	std::fputc('\n', p_level == LogLevel::Info ? stdout : stderr);

	va_end(args);
}

void fatal(const char *p_format, ...) {
	std::va_list args;
	va_start(args, p_format);
	std::fputs("[zilla][fatal] ", stderr);
	std::vfprintf(stderr, p_format, args);
	std::fputc('\n', stderr);
	va_end(args);
	std::exit(1);
}

} // namespace zilla
