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

#ifndef NOTIFY_H
#define NOTIFY_H

/*
 * This library implements inter-process signalling using blocking
 * read() on an anonymous pipe.  As a side effect, the read() system
 * call has the same signal interruption behaviour as the old sleep
 * function.
 *
 * As soon as mpd uses threading instead of fork()/shm, we can replace
 * this library with a pthread_cond object.
 *
 * This code is experimental and carries a lot of overhead.  Still, it
 * uses less resources than the old polling code with a fixed sleep
 * time.
 *
 */

typedef struct _Notify {
	int fds[2];
} Notify;

void notifyInit(Notify *notify);

void notifyWait(Notify *notify);

void notifySignal(Notify *notify);

#endif
