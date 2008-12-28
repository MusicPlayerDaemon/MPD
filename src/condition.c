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
	cond->mutex = g_mutex_new();
	if (cond->mutex == NULL)
		FATAL("g_mutex_new failed");

	cond->cond = g_cond_new();
	if (cond->cond == NULL)
		FATAL("g_cond_new() failed");
}

void cond_enter(struct condition *cond)
{
	g_mutex_lock(cond->mutex);
}

void cond_leave(struct condition *cond)
{
	g_mutex_unlock(cond->mutex);
}

void cond_wait(struct condition *cond)
{
	g_cond_wait(cond->cond, cond->mutex);
}

int cond_timedwait(struct condition *cond, const long sec)
{
	GTimeVal t;

	g_get_current_time(&t);
	g_time_val_add(&t, sec * 1000000);

	if (g_cond_timed_wait(cond->cond, cond->mutex, &t) == FALSE)
		return ETIMEDOUT;
	return 0;
}

int cond_signal_async(struct condition *cond)
{
	if (g_mutex_trylock(cond->mutex) == FALSE) {
		g_cond_signal(cond->cond);
		g_mutex_unlock(cond->mutex);
		return 0;
	}
	return EBUSY;
}

void cond_signal_sync(struct condition *cond)
{
	g_cond_signal(cond->cond);
}

void cond_destroy(struct condition *cond)
{
	g_mutex_free(cond->mutex);
	g_cond_free(cond->cond);
}
