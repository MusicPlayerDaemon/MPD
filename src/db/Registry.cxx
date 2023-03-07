// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "config.h"
#include "Registry.hxx"
#include "DatabasePlugin.hxx"
#include "plugins/simple/SimpleDatabasePlugin.hxx"
#include "plugins/ProxyDatabasePlugin.hxx"
#include "plugins/upnp/UpnpDatabasePlugin.hxx"

#include <string.h>

constinit const DatabasePlugin *const database_plugins[] = {
	&simple_db_plugin,
#ifdef ENABLE_LIBMPDCLIENT
	&proxy_db_plugin,
#endif
#ifdef ENABLE_UPNP
	&upnp_db_plugin,
#endif
	nullptr
};

const DatabasePlugin *
GetDatabasePluginByName(const char *name) noexcept
{
	for (auto i = database_plugins; *i != nullptr; ++i)
		if (strcmp((*i)->name, name) == 0)
			return *i;

	return nullptr;
}
