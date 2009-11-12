/*
 * Copyright (C) 2003-2009 The Music Player Daemon Project
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
#include "sig_handlers.h"

#ifndef WIN32

#include "log.h"
#include "main.h"
#include "event_pipe.h"

#include <glib.h>

#include <signal.h>
#include <errno.h>
#include <string.h>

static void exit_signal_handler(G_GNUC_UNUSED int signum)
{
	g_main_loop_quit(main_loop);
}

static void reload_signal_handler(G_GNUC_UNUSED int signum)
{
	event_pipe_emit_fast(PIPE_EVENT_RELOAD);
}

static void
x_sigaction(int signum, const struct sigaction *act)
{
	if (sigaction(signum, act, NULL) < 0)
		g_error("sigaction() failed: %s", strerror(errno));
}

static void
handle_reload_event(void)
{
	g_debug("got SIGHUP, reopening log files");
	cycle_log_files();
}

#endif

void initSigHandlers(void)
{
#ifndef WIN32
	struct sigaction sa;

	sa.sa_flags = 0;
	sigemptyset(&sa.sa_mask);
	sa.sa_handler = SIG_IGN;
	while (sigaction(SIGPIPE, &sa, NULL) < 0 && errno == EINTR) ;

	sa.sa_handler = exit_signal_handler;
	x_sigaction(SIGINT, &sa);
	x_sigaction(SIGTERM, &sa);

	event_pipe_register(PIPE_EVENT_RELOAD, handle_reload_event);
	sa.sa_handler = reload_signal_handler;
	x_sigaction(SIGHUP, &sa);
#endif
}
