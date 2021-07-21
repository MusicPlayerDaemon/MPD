/*
 * Copyright 2007-2020 CM4all GmbH
 * All rights reserved.
 *
 * author: Max Kellermann <mk@cm4all.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the
 * distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * FOUNDATION OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef ODBUS_WATCH_HXX
#define ODBUS_WATCH_HXX

#include "Connection.hxx"
#include "event/SocketEvent.hxx"
#include "event/DeferEvent.hxx"

#include <dbus/dbus.h>

#include <map>

class EventLoop;

namespace ODBus {

class WatchManagerObserver {
public:
	virtual void OnDBusClosed() noexcept = 0;
};

/**
 * Integrate a DBusConnection into the #EventLoop.
 */
class WatchManager {
	WatchManagerObserver &observer;

	Connection connection;

	class Watch {
		WatchManager &parent;
		DBusWatch &watch;
		SocketEvent event;

	public:
		Watch(EventLoop &event_loop, WatchManager &_parent,
		      DBusWatch &_watch) noexcept;

		void Toggled() noexcept;

	private:
		void OnSocketReady(unsigned events) noexcept;
	};

	std::map<DBusWatch *, Watch> watches;

	DeferEvent defer_dispatch;

public:
	WatchManager(EventLoop &event_loop,
		     WatchManagerObserver &_observer) noexcept
		:observer(_observer),
		 defer_dispatch(event_loop, BIND_THIS_METHOD(Dispatch))
	{
	}

	template<typename C>
	WatchManager(EventLoop &event_loop, WatchManagerObserver &_observer,
		     C &&_connection) noexcept
		:WatchManager(event_loop, _observer)
	{
		SetConnection(std::forward<C>(_connection));
	}

	~WatchManager() noexcept {
		Shutdown();
	}

	WatchManager(const WatchManager &) = delete;
	WatchManager &operator=(const WatchManager &) = delete;

	void Shutdown() noexcept;

	auto &GetEventLoop() const noexcept {
		return defer_dispatch.GetEventLoop();
	}

	Connection &GetConnection() noexcept {
		return connection;
	}

	template<typename C>
	void SetConnection(C &&_connection) noexcept {
		Shutdown();

		connection = std::forward<C>(_connection);

		if (connection)
			dbus_connection_set_watch_functions(connection,
							    AddFunction,
							    RemoveFunction,
							    ToggledFunction,
							    (void *)this,
							    nullptr);
	}

private:
	void ScheduleDispatch() noexcept {
		defer_dispatch.Schedule();
	}

	void Dispatch() noexcept;

	bool Add(DBusWatch *watch) noexcept;
	void Remove(DBusWatch *watch) noexcept;
	void Toggled(DBusWatch *watch) noexcept;

	static dbus_bool_t AddFunction(DBusWatch *watch, void *data) noexcept {
		auto &wm = *(WatchManager *)data;
		return wm.Add(watch);
	}

	static void RemoveFunction(DBusWatch *watch, void *data) noexcept {
		auto &wm = *(WatchManager *)data;
		wm.Remove(watch);
	}

	static void ToggledFunction(DBusWatch *watch, void *data) noexcept {
		auto &wm = *(WatchManager *)data;
		wm.Toggled(watch);
	}
};

} /* namespace ODBus */

#endif
