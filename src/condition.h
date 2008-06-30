/* the Music Player Daemon (MPD)
 * Copyright (C) 2003-2007 by Warren Dukes (warren.dukes@gmail.com)
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

#ifndef CONDITION_H
#define CONDITION_H

#include "os_compat.h"

struct condition {
	pthread_mutex_t mutex;
	pthread_cond_t cond;
};

void cond_init(struct condition *cond);

/**
 * The thread which shall be notified by this object must call this
 * function before any cond_wait() invocation.  It locks the mutex.
 */
void cond_enter(struct condition *cond);

/**
 * Neutralize cond_leave().
 */
void cond_leave(struct condition *cond);

/**
 * Wait for a conditio.  Return immediately if we have already
 * been notified since we last returned from cond_wait().
 */
void cond_wait(struct condition *cond);

/**
 * Wait for a condition with timeout
 *
 * @param sec number of seconds to wait for (subject to change)
 *
 * @return ETIMEDOUT if timed out, 0 if notification was received
 */
int cond_timedwait(struct condition *cond, const long sec);

/**
 * Notify the thread there is a waiter.  This function never blocks.
 *
 * @return EBUSY if it was unable to lock the mutex, 0 on success
 */
int cond_signal_async(struct condition *cond);

/**
 * Notify the thread synchronously, i.e. wait until it can deliver
 * the notification.
 */
void cond_signal_sync(struct condition *cond);

/**
 * cond_destroy - destroy the cond and internal structures
 */
void cond_destroy(struct condition *cond);

#endif /* CONDITION_H */
