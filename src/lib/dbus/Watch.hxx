// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include "Connection.hxx"
#include "event/DeferEvent.hxx"

#include <cstdint>
#include <map>

typedef uint32_t dbus_bool_t;
struct DBusWatch;
struct DBusTimeout;
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

	class Watch;
	std::map<DBusWatch *, Watch> watches;

	class Timeout;
	std::map<DBusTimeout *, Timeout> timeouts;

	DeferEvent defer_dispatch;

public:
	WatchManager(EventLoop &event_loop,
		     WatchManagerObserver &_observer) noexcept;

	template<typename C>
	WatchManager(EventLoop &event_loop, WatchManagerObserver &_observer,
		     C &&_connection) noexcept
		:WatchManager(event_loop, _observer)
	{
		SetConnection(std::forward<C>(_connection));
	}

	~WatchManager() noexcept;

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
			SetupConnection();
	}

private:
	void SetupConnection() noexcept;

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

	bool AddTimeout(DBusTimeout *timeout) noexcept;
	void RemoveTimeout(DBusTimeout *timeout) noexcept;
	void ToggledTimeout(DBusTimeout *timeout) noexcept;

	static dbus_bool_t AddTimeoutFunction(DBusTimeout *timeout,
					      void *data) noexcept {
		auto &wm = *(WatchManager *)data;
		return wm.AddTimeout(timeout);
	}

	static void RemoveTimeoutFunction(DBusTimeout *timeout,
					  void *data) noexcept {
		auto &wm = *(WatchManager *)data;
		wm.RemoveTimeout(timeout);
	}

	static void TimeoutToggledFunction(DBusTimeout *timeout,
					   void *data) noexcept {
		auto &wm = *(WatchManager *)data;
		wm.ToggledTimeout(timeout);
	}
};

} /* namespace ODBus */
