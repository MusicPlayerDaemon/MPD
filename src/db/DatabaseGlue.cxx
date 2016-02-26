/*
 * Copyright 2003-2016 The Music Player Daemon Project
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
#include "DatabaseGlue.hxx"
#include "Registry.hxx"
#include "DatabaseError.hxx"
#include "util/Error.hxx"
#include "config/Block.hxx"
#include "DatabasePlugin.hxx"

#include <string.h>

Database *
DatabaseGlobalInit(EventLoop &loop, DatabaseListener &listener,
		   const ConfigBlock &block, Error &error)
{
	const char *plugin_name =
		block.GetBlockValue("plugin", "simple");

	const DatabasePlugin *plugin = GetDatabasePluginByName(plugin_name);
	if (plugin == nullptr) {
		error.Format(db_domain,
			     "No such database plugin: %s", plugin_name);
		return nullptr;
	}

	return plugin->create(loop, listener, block, error);
}
