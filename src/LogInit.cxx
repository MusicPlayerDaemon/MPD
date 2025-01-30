// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "config.h"
#include "LogInit.hxx"
#include "LogBackend.hxx"
#include "Log.hxx"
#include "lib/fmt/PathFormatter.hxx"
#include "lib/fmt/RuntimeError.hxx"
#include "config/Param.hxx"
#include "config/Data.hxx"
#include "config/Option.hxx"
#include "fs/AllocatedPath.hxx"
#include "fs/FileSystem.hxx"
#include "util/Domain.hxx"
#include "util/StringAPI.hxx"
#include "lib/fmt/SystemError.hxx"

#include <cassert>

#ifdef ENABLE_SYSTEMD_DAEMON
#include <systemd/sd-daemon.h>
#endif

#include <string.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

#define LOG_DATE_BUF_SIZE 16
#define LOG_DATE_LEN (LOG_DATE_BUF_SIZE - 1)

[[maybe_unused]]
static constexpr Domain log_domain("log");

#ifndef ANDROID

static int out_fd = -1;
static AllocatedPath out_path = nullptr;

static void redirect_logs(int fd)
{
	assert(fd >= 0);
	if (dup2(fd, STDOUT_FILENO) < 0)
		throw MakeErrno("Failed to dup2 stdout");
	if (dup2(fd, STDERR_FILENO) < 0)
		throw MakeErrno("Failed to dup2 stderr");
}

static int
open_log_file()
{
	assert(!out_path.IsNull());

	return OpenFile(out_path, O_CREAT | O_WRONLY | O_APPEND, 0666).Steal();
}

static void
log_init_file(int line)
{
	assert(!out_path.IsNull());

	out_fd = open_log_file();
	if (out_fd < 0) {
#ifdef _WIN32
		throw FmtRuntimeError("failed to open log file {:?} (config line {})",
				      out_path, line);
#else
		throw FmtErrno("failed to open log file {:?} (config line {})",
			       out_path, line);
#endif
	}
}

static inline LogLevel
parse_log_level(const char *value)
{
	if (StringIsEqual(value, "notice") ||
	    /* deprecated name: */
	    StringIsEqual(value, "default"))
		return LogLevel::NOTICE;
	else if (StringIsEqual(value, "info") ||
		 /* deprecated since MPD 0.22: */
		 StringIsEqual(value, "secure"))
		return LogLevel::INFO;
	else if (StringIsEqual(value, "verbose"))
		return LogLevel::DEBUG;
	else if (StringIsEqual(value, "warning"))
		return LogLevel::WARNING;
	else if (StringIsEqual(value, "error"))
		return LogLevel::ERROR;
	else
		throw FmtRuntimeError("unknown log level {:?}", value);
}

static inline LogTimestamp
parse_log_timestamp(const char *value)
{
	if (StringIsEqual(value, "none"))
		return LogTimestamp::NONE;
	if (StringIsEqual(value, "minutes"))
		return LogTimestamp::MINUTES;
	else if (StringIsEqual(value, "seconds"))
		return LogTimestamp::SECONDS;
	else if (StringIsEqual(value, "milliseconds"))
		return LogTimestamp::MILLISECONDS;
	else
		throw FmtRuntimeError("unknown log timestamp {:?}. expected one of: none, minutes, seconds, milliseconds", value);
}

#endif // #ifndef ANDROID

void
log_early_init(bool verbose) noexcept
{
#ifdef ANDROID
	(void)verbose;
#else
	/* force stderr to be line-buffered */
	setvbuf(stderr, nullptr, _IOLBF, 0);

	if (verbose)
		SetLogThreshold(LogLevel::DEBUG);
#endif
}

void
log_init(const ConfigData &config, bool verbose, bool use_stdout)
{
#ifdef ANDROID
	(void)config;
	(void)verbose;
	(void)use_stdout;
#else
	if (verbose)
		SetLogThreshold(LogLevel::DEBUG);
	else
		SetLogThreshold(config.With(ConfigOption::LOG_LEVEL, [](const char *s){
			return s != nullptr
				? parse_log_level(s)
				: LogLevel::NOTICE;
		}));

	LogTimestamp log_timestamp = config.With(ConfigOption::LOG_TIMESTAMP, [](const char *s){
		return s != nullptr
		       ? parse_log_timestamp(s)
		       : LogTimestamp::SECONDS;
	});

	if (use_stdout) {
		out_fd = STDOUT_FILENO;
		EnableLogTimestamp(log_timestamp);
	} else {
		const auto *param = config.GetParam(ConfigOption::LOG_FILE);
		if (param == nullptr) {
			/* no configuration: default to syslog (if
			   available) */
#ifdef ENABLE_SYSTEMD_DAEMON
			if (sd_booted() &&
			    getenv("NOTIFY_SOCKET") != nullptr) {
				/* if MPD was started as a systemd
				   service, default to journal (which
				   is connected to stdout&stderr) */
				out_fd = STDOUT_FILENO;
				return;
			}
#endif
#ifdef _WIN32
			/* default to stdout on Windows */
			out_fd = STDOUT_FILENO;
#elif !defined(HAVE_SYSLOG)
			throw std::runtime_error("config parameter 'log_file' not found");
#endif
#ifdef HAVE_SYSLOG
		} else if (StringIsEqual(param->value.c_str(), "syslog")) {
			LogInitSysLog();
#endif
		} else {
			out_path = param->GetPath();
			EnableLogTimestamp(log_timestamp);
			log_init_file(param->line);
		}
	}
#endif
}

#ifndef ANDROID

static void
close_log_files() noexcept
{
#ifdef HAVE_SYSLOG
	LogFinishSysLog();
#endif
}

#endif

void
log_deinit() noexcept
{
#ifndef ANDROID
	close_log_files();
	out_path = nullptr;
#endif
}

void setup_log_output()
{
#ifndef ANDROID
	if (out_fd == STDOUT_FILENO)
		return;

	fflush(nullptr);

	if (out_fd < 0) {
#ifdef _WIN32
		return;
#else
		out_fd = open("/dev/null", O_WRONLY);
		if (out_fd < 0)
			return;
#endif
	}

	redirect_logs(out_fd);
	close(out_fd);
	out_fd = -1;
#endif
}

int
cycle_log_files() noexcept
{
#ifdef ANDROID
	return 0;
#else
	int fd;

	if (out_path.IsNull())
		return 0;

	LogDebug(log_domain, "Cycling log files");
	close_log_files();

	fd = open_log_file();
	if (fd < 0) {
		FmtError(log_domain,
			 "error re-opening log file: {}",
			 out_path);
		return -1;
	}

	redirect_logs(fd);
	close(fd);

	LogDebug(log_domain, "Done cycling log files");
	return 0;
#endif
}
