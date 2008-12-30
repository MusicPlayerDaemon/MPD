/*
 * Copyright (C) 2003-2008 The Music Player Daemon Project
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "daemon.h"
#include "conf.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

void
daemonize_close_stdin(void)
{
	int fd = open("/dev/null", O_RDONLY);

	if (fd < 0)
		close(STDIN_FILENO);
	else if (fd != STDIN_FILENO) {
		dup2(fd, STDIN_FILENO);
		close(fd);
	}
}

void
daemonize(Options *options)
{
#ifndef WIN32
	FILE *fp = NULL;
	ConfigParam *pidFileParam = parseConfigFilePath(CONF_PID_FILE, 0);

	if (pidFileParam) {
		/* do this before daemon'izing so we can fail gracefully if we can't
		 * write to the pid file */
		g_debug("opening pid file");
		fp = fopen(pidFileParam->value, "w+");
		if (!fp) {
			g_error("could not open %s \"%s\" (at line %i) for writing: %s",
				CONF_PID_FILE, pidFileParam->value,
				pidFileParam->line, strerror(errno));
		}
	}

	if (options->daemon) {
		int pid;

		fflush(NULL);
		pid = fork();
		if (pid > 0)
			_exit(EXIT_SUCCESS);
		else if (pid < 0) {
			g_error("problems fork'ing for daemon!");
		}

		if (chdir("/") < 0) {
			g_error("problems changing to root directory");
		}

		if (setsid() < 0) {
			g_error("problems setsid'ing");
		}

		fflush(NULL);
		pid = fork();
		if (pid > 0)
			_exit(EXIT_SUCCESS);
		else if (pid < 0) {
			g_error("problems fork'ing for daemon!");
		}

		g_debug("daemonized!");
	}

	if (pidFileParam) {
		g_debug("writing pid file");
		fprintf(fp, "%lu\n", (unsigned long)getpid());
		fclose(fp);
	}
#else
	/* no daemonization on WIN32 */
	(void)options;
#endif
}
