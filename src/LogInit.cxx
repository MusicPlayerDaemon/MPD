/*
 * Copyright (C) 2003-2014 The Music Player Daemon Project
 * http://www.musicpd.org
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "config.h"
#include "LogInit.hxx"
#include "LogBackend.hxx"
#include "Log.hxx"
#include "config/ConfigData.hxx"
#include "config/ConfigGlobal.hxx"
#include "config/ConfigOption.hxx"
#include "system/FatalError.hxx"
#include "fs/AllocatedPath.hxx"
#include "fs/FileSystem.hxx"
#include "util/Error.hxx"
#include "util/Domain.hxx"
#include "system/FatalError.hxx"

#ifdef HAVE_GLIB
#include <glib.h>
#endif

#include <assert.h>
#include <string.h>
#include <fcntl.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>

#define LOG_LEVEL_SECURE LogLevel::INFO

#define LOG_DATE_BUF_SIZE 16
#define LOG_DATE_LEN (LOG_DATE_BUF_SIZE - 1)

static constexpr Domain log_domain("log");

#ifndef ANDROID

static int out_fd;
static AllocatedPath out_path = AllocatedPath::Null();

static void redirect_logs(int fd)
{
	assert(fd >= 0);
	if (dup2(fd, STDOUT_FILENO) < 0)
		FatalSystemError("Failed to dup2 stdout");
	if (dup2(fd, STDERR_FILENO) < 0)
		FatalSystemError("Failed to dup2 stderr");
}

static int
open_log_file(void)
{
	assert(!out_path.IsNull());

	return OpenFile(out_path, O_CREAT | O_WRONLY | O_APPEND, 0666);
}

static bool
log_init_file(unsigned line, Error &error)
{
	assert(!out_path.IsNull());

	out_fd = open_log_file();
	if (out_fd < 0) {
		const std::string out_path_utf8 = out_path.ToUTF8();
		error.FormatErrno("failed to open log file \"%s\" (config line %u)",
				  out_path_utf8.c_str(), line);
		return false;
	}

	EnableLogTimestamp();
	return true;
}

static inline LogLevel
parse_log_level(const char *value, unsigned line)
{
	if (0 == strcmp(value, "default"))
		return LogLevel::DEFAULT;
	if (0 == strcmp(value, "secure"))
		return LOG_LEVEL_SECURE;
	else if (0 == strcmp(value, "verbose"))
		return LogLevel::DEBUG;
	else {
		FormatFatalError("unknown log level \"%s\" at line %u",
				 value, line);
	}
}

#endif

void
log_early_init(bool verbose)
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

bool
log_init(bool verbose, bool use_stdout, Error &error)
{
#ifdef ANDROID
	(void)verbose;
	(void)use_stdout;
	(void)error;

	return true;
#else
	const struct config_param *param;

#ifdef HAVE_GLIB
	const char *charset;
	g_get_charset(&charset);
	SetLogCharset(charset);
#endif

	if (verbose)
		SetLogThreshold(LogLevel::DEBUG);
	else if ((param = config_get_param(CONF_LOG_LEVEL)) != nullptr)
		SetLogThreshold(parse_log_level(param->value.c_str(),
						param->line));

	if (use_stdout) {
		return true;
	} else {
		param = config_get_param(CONF_LOG_FILE);
		if (param == nullptr) {
#ifdef HAVE_SYSLOG
			/* no configuration: default to syslog (if
			   available) */
			LogInitSysLog();
			return true;
#else
			error.Set(log_domain,
				  "config parameter 'log_file' not found");
			return false;
#endif
#ifdef HAVE_SYSLOG
		} else if (strcmp(param->value.c_str(), "syslog") == 0) {
			LogInitSysLog();
			return true;
#endif
		} else {
			out_path = config_get_path(CONF_LOG_FILE, error);
			return !out_path.IsNull() &&
				log_init_file(param->line, error);
		}
	}
#endif
}

#ifndef ANDROID

static void
close_log_files(void)
{
#ifdef HAVE_SYSLOG
	LogFinishSysLog();
#endif
}

#endif

void
log_deinit(void)
{
#ifndef ANDROID
	close_log_files();
	out_path = AllocatedPath::Null();
#endif
}

void setup_log_output(bool use_stdout)
{
#ifdef ANDROID
	(void)use_stdout;
#else
	if (use_stdout)
		return;

	fflush(nullptr);

	if (out_fd < 0) {
#ifdef WIN32
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

#ifdef HAVE_GLIB
	SetLogCharset(nullptr);
#endif
#endif
}

int cycle_log_files(void)
{
#ifdef ANDROID
	return 0;
#else
	int fd;

	if (out_path.IsNull())
		return 0;

	FormatDebug(log_domain, "Cycling log files");
	close_log_files();

	fd = open_log_file();
	if (fd < 0) {
		const std::string out_path_utf8 = out_path.ToUTF8();
		FormatError(log_domain,
			    "error re-opening log file: %s",
			    out_path_utf8.c_str());
		return -1;
	}

	redirect_logs(fd);
	close(fd);

	FormatDebug(log_domain, "Done cycling log files");
	return 0;
#endif
}
