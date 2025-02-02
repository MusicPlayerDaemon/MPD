// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "LogBackend.hxx"
#include "Log.hxx"
#include "util/Compiler.h"
#include "util/Domain.hxx"
#include "util/StringStrip.hxx"
#include "Version.h"
#include "config.h"

#include <fmt/chrono.h>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <chrono>
#include <functional>

#ifdef HAVE_SYSLOG
#include <syslog.h>
#endif

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

	assert(false);
	gcc_unreachable();
}

#else

static LogLevel log_threshold = LogLevel::NOTICE;

static bool enable_timestamp = false;

static constexpr size_t LOG_DATE_BUF_SIZE = std::char_traits<char>::length("Jan 22 15:43:14.000 : ") + 1;

template<typename DurationType>
static const char *
log_date(char buf[LOG_DATE_BUF_SIZE]);

static auto log_time_formatter = log_date<std::chrono::seconds>;

#ifdef HAVE_SYSLOG
static bool enable_syslog;
#endif

void
SetLogThreshold(LogLevel _threshold) noexcept
{
	log_threshold = _threshold;
}

template <typename DurationType>
inline constexpr const char *date_format_of = "";

template<>
inline constexpr const char *date_format_of<std::chrono::minutes> = "{:%b %d %H:%M} : ";
template<>
inline constexpr const char *date_format_of<std::chrono::seconds> = "{:%b %d %H:%M:%S} : ";
template<>
inline constexpr const char *date_format_of<std::chrono::milliseconds> = "{:%b %d %H:%M:%S} : ";

template<typename DurationType>
static const char *
log_date(char buf[LOG_DATE_BUF_SIZE])
{
	using namespace std::chrono;

	/*
	 * NOTE: fmt of chrono::time_point with %S writes
	 *       a floating point number of seconds if the
	 *       duration resolution is less than second
	 */
	auto [p, n] = fmt::format_to_n(buf, LOG_DATE_BUF_SIZE, date_format_of<DurationType>,
				       floor<DurationType>(system_clock::now()));
	assert(n < LOG_DATE_BUF_SIZE);
	*p = 0;
	return buf;
}

static auto
null_log_time_formatter = [](char buf[LOG_DATE_BUF_SIZE]) -> const char* {
	buf[0] = 0;
	return buf;
};

void
EnableLogTimestamp(LogTimestamp _log_time_stamp) noexcept
{
#ifdef HAVE_SYSLOG
	assert(!enable_syslog);
#endif
	assert(!enable_timestamp);

	enable_timestamp = true;

	switch (_log_time_stamp) {
		case LogTimestamp::MINUTES: log_time_formatter = log_date<std::chrono::minutes>; break;
		case LogTimestamp::SECONDS: log_time_formatter = log_date<std::chrono::seconds>; break;
		case LogTimestamp::MILLISECONDS: log_time_formatter = log_date<std::chrono::milliseconds>; break;
		case LogTimestamp::NONE: log_time_formatter = null_log_time_formatter; break;
	}
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

	assert(false);
	gcc_unreachable();
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
	char date_buf[LOG_DATE_BUF_SIZE];
	fmt::print(stderr, "{}{}: {}\n",
		   log_time_formatter(date_buf),
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
