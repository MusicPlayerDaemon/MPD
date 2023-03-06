// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#ifndef ODBUS_GLUE_HXX
#define ODBUS_GLUE_HXX

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
	void DisconnectIndirect() noexcept;

	void Connect();
	void Disconnect() noexcept;

	/* virtual methods from class ODBus::WatchManagerObserver */
	void OnDBusClosed() noexcept override;
};

}

#endif
