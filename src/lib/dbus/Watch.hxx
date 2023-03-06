// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

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
