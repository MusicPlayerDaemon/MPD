/*
 * Copyright (C) 2003-2013 The Music Player Daemon Project
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
#include "Log.hxx"
#include "ConfigData.hxx"
#include "ConfigGlobal.hxx"
#include "ConfigOption.hxx"
#include "system/fd_util.h"
#include "system/FatalError.hxx"
#include "fs/Path.hxx"
#include "fs/FileSystem.hxx"
#include "util/Error.hxx"
#include "util/Domain.hxx"
#include "system/FatalError.hxx"

#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <stdarg.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>
#include <glib.h>

#ifdef HAVE_SYSLOG
#include <syslog.h>
#endif

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "log"

#define LOG_LEVEL_SECURE G_LOG_LEVEL_INFO

#define LOG_DATE_BUF_SIZE 16
#define LOG_DATE_LEN (LOG_DATE_BUF_SIZE - 1)

static constexpr Domain log_domain("log");

static GLogLevelFlags log_threshold = G_LOG_LEVEL_MESSAGE;

static const char *log_charset;

static bool stdout_mode = true;
static int out_fd;
static Path out_path = Path::Null();

static void redirect_logs(int fd)
{
	assert(fd >= 0);
	if (dup2(fd, STDOUT_FILENO) < 0)
		FatalSystemError("Failed to dup2 stdout");
	if (dup2(fd, STDERR_FILENO) < 0)
		FatalSystemError("Failed to dup2 stderr");
}

static const char *log_date(void)
{
	static char buf[LOG_DATE_BUF_SIZE];
	time_t t = time(NULL);
	strftime(buf, LOG_DATE_BUF_SIZE, "%b %d %H:%M : ", localtime(&t));
	return buf;
}

/**
 * Determines the length of the string excluding trailing whitespace
 * characters.
 */
static int
chomp_length(const char *p)
{
	size_t length = strlen(p);

	while (length > 0 && g_ascii_isspace(p[length - 1]))
		--length;

	return (int)length;
}

static void
file_log_func(const gchar *domain,
	      GLogLevelFlags log_level,
	      const gchar *message, gcc_unused gpointer user_data)
{
	char *converted;

	if (log_level > log_threshold)
		return;

	if (log_charset != NULL) {
		converted = g_convert_with_fallback(message, -1,
						    log_charset, "utf-8",
						    NULL, NULL, NULL, NULL);
		if (converted != NULL)
			message = converted;
	} else
		converted = NULL;

	if (domain == nullptr)
		domain = "";

	fprintf(stderr, "%s%s%s%.*s\n",
		stdout_mode ? "" : log_date(),
		domain, *domain == 0 ? "" : ": ",
		chomp_length(message), message);

	g_free(converted);
}

static void
log_init_stdout(void)
{
	g_log_set_default_handler(file_log_func, NULL);
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

	g_log_set_default_handler(file_log_func, NULL);
	return true;
}

#ifdef HAVE_SYSLOG

static int
glib_to_syslog_level(GLogLevelFlags log_level)
{
	switch (log_level & G_LOG_LEVEL_MASK) {
	case G_LOG_LEVEL_ERROR:
	case G_LOG_LEVEL_CRITICAL:
		return LOG_ERR;

	case G_LOG_LEVEL_WARNING:
		return LOG_WARNING;

	case G_LOG_LEVEL_MESSAGE:
		return LOG_NOTICE;

	case G_LOG_LEVEL_INFO:
		return LOG_INFO;

	case G_LOG_LEVEL_DEBUG:
		return LOG_DEBUG;

	default:
		return LOG_NOTICE;
	}
}

static void
syslog_log_func(const gchar *domain,
		GLogLevelFlags log_level, const gchar *message,
		gcc_unused gpointer user_data)
{
	if (stdout_mode) {
		/* fall back to the file log function during
		   startup */
		file_log_func(domain, log_level,
			      message, user_data);
		return;
	}

	if (log_level > log_threshold)
		return;

	if (domain == nullptr)
		domain = "";

	syslog(glib_to_syslog_level(log_level), "%s%s%.*s",
	       domain, *domain == 0 ? "" : ": ",
	       chomp_length(message), message);
}

static void
log_init_syslog(void)
{
	assert(out_path.IsNull());

	openlog(PACKAGE, 0, LOG_DAEMON);
	g_log_set_default_handler(syslog_log_func, NULL);
}

#endif

static inline GLogLevelFlags
parse_log_level(const char *value, unsigned line)
{
	if (0 == strcmp(value, "default"))
		return G_LOG_LEVEL_MESSAGE;
	if (0 == strcmp(value, "secure"))
		return LOG_LEVEL_SECURE;
	else if (0 == strcmp(value, "verbose"))
		return G_LOG_LEVEL_DEBUG;
	else {
		FormatFatalError("unknown log level \"%s\" at line %u",
				 value, line);
		return G_LOG_LEVEL_MESSAGE;
	}
}

void
log_early_init(bool verbose)
{
	if (verbose)
		log_threshold = G_LOG_LEVEL_DEBUG;

	log_init_stdout();
}

bool
log_init(bool verbose, bool use_stdout, Error &error)
{
	const struct config_param *param;

	g_get_charset(&log_charset);

	if (verbose)
		log_threshold = G_LOG_LEVEL_DEBUG;
	else if ((param = config_get_param(CONF_LOG_LEVEL)) != NULL)
		log_threshold = parse_log_level(param->value, param->line);

	if (use_stdout) {
		log_init_stdout();
		return true;
	} else {
		param = config_get_param(CONF_LOG_FILE);
		if (param == NULL) {
#ifdef HAVE_SYSLOG
			/* no configuration: default to syslog (if
			   available) */
			log_init_syslog();
			return true;
#else
			error.Set(log_domain,
				  "config parameter 'log_file' not found");
			return false;
#endif
#ifdef HAVE_SYSLOG
		} else if (strcmp(param->value, "syslog") == 0) {
			log_init_syslog();
			return true;
#endif
		} else {
			out_path = config_get_path(CONF_LOG_FILE, error);
			return !out_path.IsNull() &&
				log_init_file(param->line, error);
		}
	}
}

static void
close_log_files(void)
{
	if (stdout_mode)
		return;

#ifdef HAVE_SYSLOG
	if (out_path.IsNull())
		closelog();
#endif
}

void
log_deinit(void)
{
	close_log_files();
	out_path = Path::Null();
}


void setup_log_output(bool use_stdout)
{
	fflush(NULL);
	if (!use_stdout) {
#ifndef WIN32
		if (out_path.IsNull())
			out_fd = open("/dev/null", O_WRONLY);
#endif

		if (out_fd >= 0) {
			redirect_logs(out_fd);
			close(out_fd);
		}

		stdout_mode = false;
		log_charset = NULL;
	}
}

int cycle_log_files(void)
{
	int fd;

	if (stdout_mode || out_path.IsNull())
		return 0;

	assert(!out_path.IsNull());

	g_debug("Cycling log files...\n");
	close_log_files();

	fd = open_log_file();
	if (fd < 0) {
		const std::string out_path_utf8 = out_path.ToUTF8();
		g_warning("error re-opening log file: %s",
			  out_path_utf8.c_str());
		return -1;
	}

	redirect_logs(fd);
	g_debug("Done cycling log files\n");
	return 0;
}
