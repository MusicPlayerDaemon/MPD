// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#ifndef ODBUS_INIT_HXX
#define ODBUS_INIT_HXX

#include <dbus/dbus.h>

namespace ODBus {

class ScopeInit {
public:
	ScopeInit() = default;

	~ScopeInit() noexcept {
		/* free libdbus memory to make memory leak checkers happy */
		dbus_shutdown();
	}

	ScopeInit(const ScopeInit &) = delete;
	ScopeInit &operator=(const ScopeInit &) = delete;
};

} /* namespace ODBus */

#endif
