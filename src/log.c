/* the Music Player Daemon (MPD)
 * Copyright (C) 2003-2007 by Warren Dukes (warren.dukes@gmail.com)
 * This project's homepage is: http://www.musicpd.org
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "log.h"
#include "conf.h"

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
#include <pthread.h>
#include <glib.h>

#define LOG_LEVEL_SECURE G_LOG_LEVEL_MESSAGE

#define LOG_DATE_BUF_SIZE 16
#define LOG_DATE_LEN (LOG_DATE_BUF_SIZE - 1)

static unsigned int log_threshold = G_LOG_LEVEL_INFO;

static int stdout_mode = 1;
static int out_fd = -1;
static int err_fd = -1;
static const char *out_filename;
static const char *err_filename;

static void redirect_logs(void)
{
	assert(out_fd >= 0);
	assert(err_fd >= 0);
	if (dup2(out_fd, STDOUT_FILENO) < 0)
		FATAL("problems dup2 stdout : %s\n", strerror(errno));
	if (dup2(err_fd, STDERR_FILENO) < 0)
		FATAL("problems dup2 stderr : %s\n", strerror(errno));
}

static const char *log_date(void)
{
	static char buf[LOG_DATE_BUF_SIZE];
	time_t t = time(NULL);
	strftime(buf, LOG_DATE_BUF_SIZE, "%b %d %H:%M : ", localtime(&t));
	return buf;
}

static void
mpd_log_func(G_GNUC_UNUSED const gchar *log_domain,
	     G_GNUC_UNUSED GLogLevelFlags log_level,
	     const gchar *message, G_GNUC_UNUSED gpointer user_data)
{
	FILE *file = log_level <= G_LOG_LEVEL_WARNING
		? stderr : stdout;

	fprintf(file, "%s%s",
		stdout_mode ? "" : log_date(),
		message);
}

void initLog(const int verbose)
{
	ConfigParam *param;

	g_log_set_default_handler(mpd_log_func, NULL);

	/* unbuffer stdout, stderr is unbuffered by default, leave it */
	setvbuf(stdout, (char *)NULL, _IONBF, 0);

	if (verbose) {
		log_threshold = G_LOG_LEVEL_DEBUG;
		return;
	}
	if (!(param = getConfigParam(CONF_LOG_LEVEL)))
		return;
	if (0 == strcmp(param->value, "default")) {
		log_threshold = G_LOG_LEVEL_INFO;
	} else if (0 == strcmp(param->value, "secure")) {
		log_threshold = LOG_LEVEL_SECURE;
	} else if (0 == strcmp(param->value, "verbose")) {
		log_threshold = G_LOG_LEVEL_DEBUG;
	} else {
		FATAL("unknown log level \"%s\" at line %i\n",
		      param->value, param->line);
	}
}

void open_log_files(const int use_stdout)
{
	mode_t prev;
	ConfigParam *param;

	if (use_stdout)
		return;

	prev = umask(0066);
	param = parseConfigFilePath(CONF_LOG_FILE, 1);
	out_filename = param->value;
	out_fd = open(out_filename, O_CREAT | O_WRONLY | O_APPEND, 0666);
	if (out_fd < 0)
		FATAL("problem opening log file \"%s\" (config line %i) for "
		      "writing\n", param->value, param->line);

	param = parseConfigFilePath(CONF_ERROR_FILE, 1);
	err_filename = param->value;
	err_fd = open(err_filename, O_CREAT | O_WRONLY | O_APPEND, 0666);
	if (err_fd < 0)
		FATAL("problem opening error file \"%s\" (config line %i) for "
		      "writing\n", param->value, param->line);

	umask(prev);
}

void setup_log_output(const int use_stdout)
{
	fflush(NULL);
	if (!use_stdout) {
		redirect_logs();
		stdout_mode = 0;
	}
}

#define log_func(func,level) \
mpd_printf void func(const char *fmt, ...) \
{ \
	if ((int)log_threshold <= level) {		\
		va_list args; \
		va_start(args, fmt); \
		g_logv(NULL, level, fmt, args);	\
		va_end(args); \
	} \
}

log_func(ERROR, G_LOG_LEVEL_WARNING)
log_func(WARNING, G_LOG_LEVEL_WARNING)
log_func(LOG, G_LOG_LEVEL_MESSAGE)
log_func(SECURE, LOG_LEVEL_SECURE)
log_func(DEBUG, G_LOG_LEVEL_DEBUG)

#undef log_func

mpd_printf mpd_noreturn void FATAL(const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	g_logv(NULL, G_LOG_LEVEL_ERROR, fmt, args);
	va_end(args);
	exit(EXIT_FAILURE);
}

int cycle_log_files(void)
{
	mode_t prev;

	if (stdout_mode)
		return 0;
	assert(out_filename);
	assert(err_filename);

	DEBUG("Cycling log files...\n");
	close_log_files();

	prev = umask(0066);

	out_fd = open(out_filename, O_CREAT | O_WRONLY | O_APPEND, 0666);
	if (out_fd < 0) {
		ERROR("error re-opening log file: %s\n", out_filename);
		return -1;
	}

	err_fd = open(err_filename, O_CREAT | O_WRONLY | O_APPEND, 0666);
	if (err_fd < 0) {
		ERROR("error re-opening error file: %s\n", err_filename);
		return -1;
	}

	umask(prev);

	redirect_logs();
	DEBUG("Done cycling log files\n");
	return 0;
}

void close_log_files(void)
{
	if (stdout_mode)
		return;
	assert(out_fd >= 0);
	assert(err_fd >= 0);
	close(out_fd);
	close(err_fd);
}

