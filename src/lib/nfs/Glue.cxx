/*
 * Copyright (C) 2003-2014 The Music Player Daemon Project
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
#include "Glue.hxx"
#include "Manager.hxx"
#include "IOThread.hxx"
#include "event/Call.hxx"
#include "util/Manual.hxx"

#include <assert.h>

static Manual<NfsManager> nfs_glue;
static unsigned in_use;

void
nfs_init()
{
	if (in_use++ > 0)
		return;

	nfs_glue.Construct(io_thread_get());
}

void
nfs_finish()
{
	assert(in_use > 0);

	if (--in_use > 0)
		return;

	BlockingCall(io_thread_get(), [](){ nfs_glue.Destruct(); });
}

NfsConnection &
nfs_get_connection(const char *server, const char *export_name)
{
	assert(in_use > 0);
	assert(io_thread_inside());

	return nfs_glue->GetConnection(server, export_name);
}
