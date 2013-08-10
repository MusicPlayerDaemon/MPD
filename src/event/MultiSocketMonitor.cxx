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
#include "MultiSocketMonitor.hxx"
#include "Loop.hxx"
#include "system/fd_util.h"
#include "gcc.h"

#include <assert.h>

/**
 * The vtable for our GSource implementation.  Unfortunately, we
 * cannot declare it "const", because g_source_new() takes a non-const
 * pointer, for whatever reason.
 */
static GSourceFuncs multi_socket_monitor_source_funcs = {
	MultiSocketMonitor::Prepare,
	MultiSocketMonitor::Check,
	MultiSocketMonitor::Dispatch,
	nullptr,
	nullptr,
	nullptr,
};

MultiSocketMonitor::MultiSocketMonitor(EventLoop &_loop)
	:loop(_loop),
	 source((Source *)g_source_new(&multi_socket_monitor_source_funcs,
				       sizeof(*source))),
	 absolute_timeout_us(G_MAXINT64) {
	source->monitor = this;

	g_source_attach(&source->base, loop.GetContext());
}

MultiSocketMonitor::~MultiSocketMonitor()
{
	g_source_destroy(&source->base);
	g_source_unref(&source->base);
	source = nullptr;
}

bool
MultiSocketMonitor::Prepare(gint *timeout_r)
{
	int timeout_ms = *timeout_r = PrepareSockets();
	absolute_timeout_us = timeout_ms < 0
		? G_MAXINT64
		: GetTime() + gint64(timeout_ms) * 1000;

	return false;
}

bool
MultiSocketMonitor::Check() const
{
	if (GetTime() >= absolute_timeout_us)
		return true;

	for (const auto &i : fds)
		if (i.revents != 0)
			return true;

	return false;
}

/*
 * GSource methods
 *
 */

gboolean
MultiSocketMonitor::Prepare(GSource *_source, gint *timeout_r)
{
	Source &source = *(Source *)_source;
	MultiSocketMonitor &monitor = *source.monitor;
	assert(_source == &monitor.source->base);

	return monitor.Prepare(timeout_r);
}

gboolean
MultiSocketMonitor::Check(GSource *_source)
{
	const Source &source = *(const Source *)_source;
	const MultiSocketMonitor &monitor = *source.monitor;
	assert(_source == &monitor.source->base);

	return monitor.Check();
}

gboolean
MultiSocketMonitor::Dispatch(GSource *_source,
			     gcc_unused GSourceFunc callback,
			     gcc_unused gpointer user_data)
{
	Source &source = *(Source *)_source;
	MultiSocketMonitor &monitor = *source.monitor;
	assert(_source == &monitor.source->base);

	monitor.Dispatch();
	return true;
}
