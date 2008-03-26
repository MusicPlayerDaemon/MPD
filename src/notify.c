/* the Music Player Daemon (MPD)
 * Copyright (C) 2008 Max Kellermann <max@duempel.org>
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

#include "notify.h"
#include "os_compat.h"
#include "log.h"
#include "utils.h"

void initNotify(Notify *notify)
{
	if (pipe(notify->fds) < 0)
		FATAL("Couldn't open pipe: %s", strerror(errno));
	if (set_nonblocking(notify->fds[1]) < 0)
		FATAL("Couldn't set non-blocking on notify fd: %s",
		      strerror(errno));
}

void waitNotify(Notify *notify)
{
	char buffer[64];

	if (read(notify->fds[0], buffer, sizeof(buffer)) < 0)
		FATAL("error reading from pipe: %s\n", strerror(errno));
}

void signalNotify(Notify *notify)
{
	char buffer;

	if (write(notify->fds[1], &buffer, sizeof(buffer)) < 0 &&
	    errno != EAGAIN && errno != EINTR)
		FATAL("error writing to pipe: %s\n", strerror(errno));
}
