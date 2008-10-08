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

#include "condition.h"
#include "utils.h"
#include "log.h"

#include <sys/time.h>
#include <string.h>

void cond_init(struct condition *cond)
{
	xpthread_mutex_init(&cond->mutex, NULL);
	xpthread_cond_init(&cond->cond, NULL);
}

void cond_enter(struct condition *cond)
{
	pthread_mutex_lock(&cond->mutex);
}

void cond_leave(struct condition *cond)
{
	pthread_mutex_unlock(&cond->mutex);
}

void cond_wait(struct condition *cond)
{
	pthread_cond_wait(&cond->cond, &cond->mutex);
}

static struct timespec * ts_timeout(struct timespec *ts, const long sec)
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	ts->tv_sec = tv.tv_sec + sec;
	ts->tv_nsec = tv.tv_usec * 1000;
	return ts;
}

int cond_timedwait(struct condition *cond, const long sec)
{
	struct timespec ts;
	int ret = pthread_cond_timedwait(&cond->cond, &cond->mutex,
	                                 ts_timeout(&ts, sec));
	if (!ret || ret == ETIMEDOUT)
		return ret;
	FATAL("cond_timedwait: %s\n", strerror(ret));
	return ret;
}

int cond_signal_async(struct condition *cond)
{
	if (!pthread_mutex_trylock(&cond->mutex)) {
		pthread_cond_signal(&cond->cond);
		pthread_mutex_unlock(&cond->mutex);
		return 0;
	}
	return EBUSY;
}

void cond_signal_sync(struct condition *cond)
{
	pthread_cond_signal(&cond->cond);
}

void cond_destroy(struct condition *cond)
{
	xpthread_cond_destroy(&cond->cond);
	xpthread_mutex_destroy(&cond->mutex);
}
