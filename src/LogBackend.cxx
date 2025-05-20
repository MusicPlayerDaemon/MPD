// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "LogBackend.hxx"
#include "Log.hxx"
#include "lib/fmt/Unsafe.hxx"
#include "util/Domain.hxx"
#include "util/StringStrip.hxx"
#include "Version.h"
#include "config.h"

#include <fmt/chrono.h>

#include <cassert>
#include <utility> // for std::unreachable()

#include <stdio.h>
#include <string.h>
#include <time.h>

#ifdef HAVE_SYSLOG
#include <syslog.h>
#endif

using std::string_view_literals::operator""sv;

#ifdef ANDROID
#include <android/log.h>
#include "android/LogListener.hxx"
#include "Main.hxx"

static int
ToAndroidLogLevel(LogLevel log_level) noexcept
{
	switch (log_level) {
	case LogLevel::DEBUG:
		return ANDROID_LOG_DEBUG;

	case LogLevel::INFO:
	case LogLevel::NOTICE:
		return ANDROID_LOG_INFO;

	case LogLevel::WARNING:
		return ANDROID_LOG_WARN;

	case LogLevel::ERROR:
		return ANDROID_LOG_ERROR;
	}

	std::unreachable();
}

#else

static LogLevel log_threshold = LogLevel::NOTICE;

static bool enable_timestamp;

#ifdef HAVE_SYSLOG
static bool enable_syslog;
#endif

void
SetLogThreshold(LogLevel _threshold) noexcept
{
	log_threshold = _threshold;
}

void
EnableLogTimestamp() noexcept
{
#ifdef HAVE_SYSLOG
	assert(!enable_syslog);
#endif
	assert(!enable_timestamp);

	enable_timestamp = true;
}

static std::string_view
log_date() noexcept
{
	static constexpr size_t LOG_DATE_BUF_SIZE = std::char_traits<char>::length("2025-01-22T15:43:14 ") + 1;
	static char buf[LOG_DATE_BUF_SIZE];
	time_t t = time(nullptr);
	const auto *tm = std::localtime(&t);
	if (tm == nullptr)
		return {};

	return FmtUnsafeSV(buf, "{:%FT%T} "sv, *tm);
}

#ifdef HAVE_SYSLOG

/**
 * Determines the length of the string excluding trailing whitespace
 * characters.
 */
static int
chomp_length(std::string_view p) noexcept
{
	return StripRight(p.data(), p.size());
}

[[gnu::const]]
static int
ToSysLogLevel(LogLevel log_level) noexcept
{
	switch (log_level) {
	case LogLevel::DEBUG:
		return LOG_DEBUG;

	case LogLevel::INFO:
		return LOG_INFO;

	case LogLevel::NOTICE:
		return LOG_NOTICE;

	case LogLevel::WARNING:
		return LOG_WARNING;

	case LogLevel::ERROR:
		return LOG_ERR;
	}

	std::unreachable();
}

static void
SysLog(const Domain &domain, LogLevel log_level, std::string_view message) noexcept
{
	syslog(ToSysLogLevel(log_level), "%s: %.*s",
	       domain.GetName(),
	       chomp_length(message), message.data());
}

void
LogInitSysLog() noexcept
{
	openlog(PACKAGE, 0, LOG_DAEMON);
	enable_syslog = true;
}

void
LogFinishSysLog() noexcept
{
	if (enable_syslog)
		closelog();
}

#endif

static void
FileLog(const Domain &domain, std::string_view message) noexcept
{
	fmt::print(stderr, "{}{}: {}\n",
		   enable_timestamp ? log_date() : ""sv,
		   domain.GetName(),
		   StripRight(message));

#ifdef _WIN32
	/* force-flush the log file, because setvbuf() does not seem
	   to have an effect on WIN32 */
	fflush(stderr);
#endif
}

#endif /* !ANDROID */

void
Log(LogLevel level, const Domain &domain, std::string_view msg) noexcept
{
#ifdef ANDROID
	__android_log_print(ToAndroidLogLevel(level), "MPD",
			    "%s: %.*s", domain.GetName(),
			    (int)msg.size(), msg.data());
	if (logListener != nullptr) {
		char buffer[1024];
		snprintf(buffer, sizeof(buffer), "%s: %.*s",
			 domain.GetName(), (int)msg.size(), msg.data());

		logListener->OnLog(Java::GetEnv(), ToAndroidLogLevel(level),
				   buffer);
	}
#else

	if (level < log_threshold)
		return;

#ifdef HAVE_SYSLOG
	if (enable_syslog) {
		SysLog(domain, level, msg);
		return;
	}
#endif

	FileLog(domain, msg);
#endif /* !ANDROID */
}
