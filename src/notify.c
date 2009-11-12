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
#include "notify.h"

void notify_init(struct notify *notify)
{
	notify->mutex = g_mutex_new();
	notify->cond = g_cond_new();
	notify->pending = false;
}

void notify_deinit(struct notify *notify)
{
	g_mutex_free(notify->mutex);
	g_cond_free(notify->cond);
}

void notify_wait(struct notify *notify)
{
	g_mutex_lock(notify->mutex);
	while (!notify->pending)
		g_cond_wait(notify->cond, notify->mutex);
	notify->pending = false;
	g_mutex_unlock(notify->mutex);
}

void notify_signal(struct notify *notify)
{
	g_mutex_lock(notify->mutex);
	notify->pending = true;
	g_cond_signal(notify->cond);
	g_mutex_unlock(notify->mutex);
}
