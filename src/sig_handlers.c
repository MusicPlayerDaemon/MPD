/* the Music Player Daemon (MPD)
 * Copyright (C) 2003-2007 by Warren Dukes (warren.dukes@gmail.com)
 * (c) 2004 Nick Welch (mack@incise.org)
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

#include "sig_handlers.h"
#include "playlist.h"
#include "database.h"
#include "update.h"
#include "command.h"
#include "signal_check.h"
#include "log.h"

#include <sys/signal.h>
#include <sys/wait.h>
#include <errno.h>
#include <glib.h>

int handlePendingSignals(void)
{
	if (signal_is_pending(SIGINT) || signal_is_pending(SIGTERM)) {
		DEBUG("main process got SIGINT or SIGTERM, exiting\n");
		return COMMAND_RETURN_KILL;
	}

	if (signal_is_pending(SIGHUP)) {
		DEBUG("got SIGHUP, rereading DB\n");
		signal_clear(SIGHUP);
		if (!isUpdatingDB()) {
			db_load();
			playlistVersionChange();
		}
		if (cycle_log_files() < 0)
			return COMMAND_RETURN_KILL;
	}

	return 0;
}

static void chldSigHandler(G_GNUC_UNUSED int sig)
{
	int status;
	int pid;
	/* DEBUG("main process got SIGCHLD\n"); */
	while (0 != (pid = wait3(&status, WNOHANG, NULL))) {
		if (pid < 0) {
			if (errno == EINTR)
				continue;
			else
				break;
		}
	}
}

void initSigHandlers(void)
{
	struct sigaction sa;

	sa.sa_flags = 0;
	sigemptyset(&sa.sa_mask);
	sa.sa_handler = SIG_IGN;
	while (sigaction(SIGPIPE, &sa, NULL) < 0 && errno == EINTR) ;
	sa.sa_handler = chldSigHandler;
	while (sigaction(SIGCHLD, &sa, NULL) < 0 && errno == EINTR) ;
	signal_handle(SIGUSR1);
	signal_handle(SIGINT);
	signal_handle(SIGTERM);
	signal_handle(SIGHUP);
}
