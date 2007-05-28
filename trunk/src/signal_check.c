/* the Music Player Daemon (MPD)
 * Copyright (C) 2003-2007 by Warren Dukes (warren.dukes@gmail.com)
 * (c)2004 by mackstann
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

#include "signal_check.h"

#include <errno.h>
#include <stddef.h>

static volatile sig_atomic_t __caught_signals[NSIG];

static void __signal_handler(int sig)
{
	__caught_signals[sig] = 1;
}

static void __set_signal_handler(int sig, void (*handler) (int))
{
	struct sigaction act;
	act.sa_flags = 0;
	sigemptyset(&act.sa_mask);
	act.sa_handler = handler;
	while (sigaction(sig, &act, NULL) && errno == EINTR) ;
}

void signal_handle(int sig)
{
	__set_signal_handler(sig, __signal_handler);
}

void signal_unhandle(int sig)
{
	signal_clear(sig);
	__set_signal_handler(sig, SIG_DFL);
}

int signal_is_pending(int sig)
{
	return __caught_signals[sig];
}

void signal_clear(int sig)
{
	__caught_signals[sig] = 0;
}
