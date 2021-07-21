/*
 * Copyright 2003-2021 The Music Player Daemon Project
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
#include "Registry.hxx"
#include "DatabasePlugin.hxx"
#include "plugins/simple/SimpleDatabasePlugin.hxx"
#include "plugins/ProxyDatabasePlugin.hxx"
#include "plugins/upnp/UpnpDatabasePlugin.hxx"

#include <string.h>

constexpr const DatabasePlugin *database_plugins[] = {
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
