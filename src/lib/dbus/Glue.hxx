/*
 * Copyright 2003-2019 The Music Player Daemon Project
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

#ifndef MPD_DBUS_GLUE_HXX
#define MPD_DBUS_GLUE_HXX

#include "Watch.hxx"

class EventLoop;

namespace ODBus {

/**
 * A class which manages the D-Bus client connection.
 */
class Glue final : ODBus::WatchManagerObserver {
	WatchManager watch;

public:
	explicit Glue(EventLoop &event_loop)
		:watch(event_loop, *this) {
		ConnectIndirect();
	}

	~Glue() noexcept {
		DisconnectIndirect();
	}

	auto &GetEventLoop() const noexcept {
		return watch.GetEventLoop();
	}

	Connection &GetConnection() noexcept {
		return watch.GetConnection();
	}

private:
	void ConnectIndirect();
	void DisconnectIndirect();

	void Connect();
	void Disconnect();

	/* virtual methods from class ODBus::WatchManagerObserver */
	void OnDBusClosed() noexcept override;
};

}

#endif
