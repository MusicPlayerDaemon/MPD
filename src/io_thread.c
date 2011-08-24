/*
 * Copyright (C) 2003-2011 The Music Player Daemon Project
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

#include "io_thread.h"

#include <assert.h>

static GMainContext *io_context;
static GMainLoop *io_loop;
static GThread *io_thread;

static gpointer
io_thread_func(G_GNUC_UNUSED gpointer arg)
{
	assert(io_context != NULL);
	assert(io_loop != NULL);

	g_main_loop_run(io_loop);
	return NULL;
}

void
io_thread_init(void)
{
	assert(io_context == NULL);
	assert(io_loop == NULL);
	assert(io_thread == NULL);

	io_context = g_main_context_new();
	io_loop = g_main_loop_new(io_context, false);
}

bool
io_thread_start(GError **error_r)
{
	assert(io_context != NULL);
	assert(io_loop != NULL);
	assert(io_thread == NULL);

	io_thread = g_thread_create(io_thread_func, NULL, true, error_r);
	if (io_thread == NULL)
		return false;

	return true;
}

void
io_thread_deinit(void)
{
	if (io_thread != NULL) {
		assert(io_loop != NULL);

		g_main_loop_quit(io_loop);
		g_thread_join(io_thread);
	}

	if (io_loop != NULL)
		g_main_loop_unref(io_loop);

	if (io_context != NULL)
		g_main_context_unref(io_context);
}

GMainContext *
io_thread_context(void)
{
	return io_context;
}

