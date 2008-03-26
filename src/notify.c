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

#include <assert.h>
#include <fcntl.h>
#include <unistd.h>

int set_nonblock(int fd)
{
	int ret;

	assert(fd >= 0);

	ret = fcntl(fd, F_GETFL, 0);
	if (ret < 0)
		return ret;

	return fcntl(fd, F_SETFL, ret|O_NONBLOCK);
}

int initNotify(Notify *notify)
{
	int ret;

	ret = pipe(notify->fds);
	if (ret < 0)
		return -1;

	ret = set_nonblock(notify->fds[1]);
	if (ret < 0) {
		close(notify->fds[0]);
		close(notify->fds[1]);
		return -1;
	}

	return 0;
}

int waitNotify(Notify *notify)
{
	char buffer[64];
	ssize_t nbytes;

	nbytes = read(notify->fds[0], buffer, sizeof(buffer));
	return (int)nbytes;
}

int signalNotify(Notify *notify)
{
	char buffer[1] = { 0 };

	return (int)write(notify->fds[1], &buffer, sizeof(buffer));
}
