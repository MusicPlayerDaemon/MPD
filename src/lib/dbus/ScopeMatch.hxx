// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include <dbus/dbus.h>

namespace ODBus {

/**
 * RAII-style wrapper for dbus_bus_add_match() and
 * dbus_bus_remove_match().
 */
class ScopeMatch {
	DBusConnection *const connection;
	const char *const rule;

public:
	ScopeMatch(DBusConnection *_connection, const char *_rule);

	~ScopeMatch() noexcept {
		dbus_bus_remove_match(connection, rule, nullptr);
	}

	ScopeMatch(const ScopeMatch &) = delete;
	ScopeMatch &operator=(const ScopeMatch &) = delete;
};

} /* namespace ODBus */
