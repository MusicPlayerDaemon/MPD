// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "LogBackend.hxx"
#include "Log.hxx"
#include "util/Compiler.h"
#include "util/Domain.hxx"
#include "util/StringStrip.hxx"
#include "Version.h"
#include "config.h"

#include <cassert>

#include <stdio.h>
#include <string.h>
#include <time.h>

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

static constexpr size_t LOG_DATE_BUF_SIZE = std::char_traits<char>::length("Jan 22 15:43:14 : ") + 1;

static const char *
log_date(char buf[LOG_DATE_BUF_SIZE]) noexcept
{
	time_t t = time(nullptr);
	strftime(buf, LOG_DATE_BUF_SIZE, "%b %d %H:%M:%S : ", localtime(&t));
	return buf;
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
		   enable_timestamp ? log_date(date_buf) : "",
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
