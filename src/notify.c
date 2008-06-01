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

int notify_init(Notify *notify)
{
	int ret;

	ret = pthread_mutex_init(&notify->mutex, NULL);
	if (ret != 0)
		return ret;

	ret = pthread_cond_init(&notify->cond, NULL);
	if (ret != 0) {
		pthread_mutex_destroy(&notify->mutex);
		return ret;
	}

	notify->pending = 0;

	return 0;
}

void notify_enter(Notify *notify)
{
	pthread_mutex_lock(&notify->mutex);
}

void notify_leave(Notify *notify)
{
	pthread_mutex_unlock(&notify->mutex);
}

void notify_wait(Notify *notify)
{
	if (!notify->pending)
		pthread_cond_wait(&notify->cond, &notify->mutex);
	notify->pending = 0;
}

void notify_signal(Notify *notify)
{
	notify->pending = 1;
	pthread_cond_signal(&notify->cond);
}

void notify_signal_sync(Notify *notify)
{
	pthread_mutex_lock(&notify->mutex);
	notify_signal(notify);
	pthread_mutex_unlock(&notify->mutex);
}
