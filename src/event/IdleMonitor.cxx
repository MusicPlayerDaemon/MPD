/*
 * Copyright (C) 2003-2013 The Music Player Daemon Project
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
#include "IdleMonitor.hxx"
#include "Loop.hxx"

void
IdleMonitor::Cancel()
{
	assert(loop.IsInside());

	if (!IsActive())
		return;

#ifdef USE_EPOLL
	active = false;
	loop.RemoveIdle(*this);
#else
	g_source_remove(source_id);
	source_id = 0;
#endif
}

void
IdleMonitor::Schedule()
{
	assert(loop.IsInside());

	if (IsActive())
		/* already scheduled */
		return;

#ifdef USE_EPOLL
	active = true;
	loop.AddIdle(*this);
#else
	source_id = loop.AddIdle(Callback, this);
#endif
}

void
IdleMonitor::Run()
{
	assert(loop.IsInside());

#ifdef USE_EPOLL
	assert(active);
	active = false;
#else
	assert(source_id != 0);
	source_id = 0;
#endif

	OnIdle();
}

#ifndef USE_EPOLL

gboolean
IdleMonitor::Callback(gpointer data)
{
	IdleMonitor &monitor = *(IdleMonitor *)data;
	monitor.Run();
	return false;
}

#endif
