// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "ScopeMatch.hxx"
#include "Error.hxx"

ODBus::ScopeMatch::ScopeMatch(DBusConnection *_connection, const char *_rule)
	:connection(_connection), rule(_rule)
{
	Error error;
	dbus_bus_add_match(connection, rule, error);
	error.CheckThrow("DBus AddMatch error");
}
