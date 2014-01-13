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
#include "GlobalEvents.hxx"
#include "util/Manual.hxx"
#include "event/DeferredMonitor.hxx"

#include <atomic>

#include <assert.h>

namespace GlobalEvents {
	class Monitor final : public DeferredMonitor {
	public:
		Monitor(EventLoop &_loop):DeferredMonitor(_loop) {}

	protected:
		virtual void RunDeferred() override;
	};

	static Manual<Monitor> monitor;

	static std::atomic_uint flags;
	static Handler handlers[MAX];
}

/**
 * Invoke the callback for a certain event.
 */
static void
InvokeGlobalEvent(GlobalEvents::Event event)
{
	assert((unsigned)event < GlobalEvents::MAX);
	assert(GlobalEvents::handlers[event] != nullptr);

	GlobalEvents::handlers[event]();
}

void
GlobalEvents::Monitor::RunDeferred()
{
	const unsigned f = flags.exchange(0);

	for (unsigned i = 0; i < MAX; ++i)
		if (f & (1u << i))
			/* invoke the event handler */
			InvokeGlobalEvent(Event(i));
}

void
GlobalEvents::Initialize(EventLoop &loop)
{
	monitor.Construct(loop);
}

void
GlobalEvents::Deinitialize()
{
	monitor.Destruct();
}

void
GlobalEvents::Register(Event event, Handler callback)
{
	assert((unsigned)event < MAX);
	assert(handlers[event] == nullptr);

	handlers[event] = callback;
}

void
GlobalEvents::Emit(Event event)
{
	assert((unsigned)event < MAX);

	const unsigned mask = 1u << unsigned(event);
	if (GlobalEvents::flags.fetch_or(mask) == 0)
		monitor->Schedule();
}
