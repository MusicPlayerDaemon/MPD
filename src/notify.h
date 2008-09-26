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

#include "os_compat.h"

typedef struct notify {
	pthread_mutex_t mutex;
	pthread_cond_t cond;
	int pending;
} Notify;

#define NOTIFY_INITIALIZER { \
	.mutex = PTHREAD_MUTEX_INITIALIZER, \
	.cond = PTHREAD_COND_INITIALIZER, \
}

void notify_init(struct notify *notify);

void notify_deinit(struct notify *notify);

/**
 * Wait for a notification.  Return immediately if we have already
 * been notified since we last returned from notify_wait().
 */
void notify_wait(struct notify *notify);

/**
 * Notify the thread.  This function never blocks.
 */
void notify_signal(struct notify *notify);

#endif
