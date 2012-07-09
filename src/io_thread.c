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

static struct {
	GMutex *mutex;
	GCond *cond;

	GMainContext *context;
	GMainLoop *loop;
	GThread *thread;
} io;

void
io_thread_run(void)
{
	assert(io_thread_inside());
	assert(io.context != NULL);
	assert(io.loop != NULL);

	g_main_loop_run(io.loop);
}

static gpointer
io_thread_func(G_GNUC_UNUSED gpointer arg)
{
	/* lock+unlock to synchronize with io_thread_start(), to be
	   sure that io.thread is set */
	g_mutex_lock(io.mutex);
	g_mutex_unlock(io.mutex);

	io_thread_run();
	return NULL;
}

void
io_thread_init(void)
{
	assert(io.context == NULL);
	assert(io.loop == NULL);
	assert(io.thread == NULL);

	io.mutex = g_mutex_new();
	io.cond = g_cond_new();
	io.context = g_main_context_new();
	io.loop = g_main_loop_new(io.context, false);
}

bool
io_thread_start(GError **error_r)
{
	assert(io.context != NULL);
	assert(io.loop != NULL);
	assert(io.thread == NULL);

	g_mutex_lock(io.mutex);
	io.thread = g_thread_create(io_thread_func, NULL, true, error_r);
	g_mutex_unlock(io.mutex);
	if (io.thread == NULL)
		return false;

	return true;
}

void
io_thread_quit(void)
{
	assert(io.loop != NULL);

	g_main_loop_quit(io.loop);
}

void
io_thread_deinit(void)
{
	if (io.thread != NULL) {
		io_thread_quit();

		g_thread_join(io.thread);
	}

	if (io.loop != NULL)
		g_main_loop_unref(io.loop);

	if (io.context != NULL)
		g_main_context_unref(io.context);

	g_cond_free(io.cond);
	g_mutex_free(io.mutex);
}

GMainContext *
io_thread_context(void)
{
	return io.context;
}

bool
io_thread_inside(void)
{
	return io.thread != NULL && g_thread_self() == io.thread;
}

guint
io_thread_idle_add(GSourceFunc function, gpointer data)
{
	GSource *source = g_idle_source_new();
	g_source_set_callback(source, function, data, NULL);
	guint id = g_source_attach(source, io.context);
	g_source_unref(source);
	return id;
}

GSource *
io_thread_timeout_add(guint interval_ms, GSourceFunc function, gpointer data)
{
	GSource *source = g_timeout_source_new(interval_ms);
	g_source_set_callback(source, function, data, NULL);
	g_source_attach(source, io.context);
	return source;
}

GSource *
io_thread_timeout_add_seconds(guint interval,
			      GSourceFunc function, gpointer data)
{
	GSource *source = g_timeout_source_new_seconds(interval);
	g_source_set_callback(source, function, data, NULL);
	g_source_attach(source, io.context);
	return source;
}

struct call_data {
	GThreadFunc function;
	gpointer data;
	bool done;
	gpointer result;
};

static gboolean
io_thread_call_func(gpointer _data)
{
	struct call_data *data = _data;

	gpointer result = data->function(data->data);

	g_mutex_lock(io.mutex);
	data->done = true;
	data->result = result;
	g_cond_broadcast(io.cond);
	g_mutex_unlock(io.mutex);

	return false;
}

gpointer
io_thread_call(GThreadFunc function, gpointer _data)
{
	assert(io.thread != NULL);

	if (io_thread_inside())
		/* we're already in the I/O thread - no
		   synchronization needed */
		return function(_data);

	struct call_data data = {
		.function = function,
		.data = _data,
		.done = false,
	};

	io_thread_idle_add(io_thread_call_func, &data);

	g_mutex_lock(io.mutex);
	while (!data.done)
		g_cond_wait(io.cond, io.mutex);
	g_mutex_unlock(io.mutex);

	return data.result;
}
