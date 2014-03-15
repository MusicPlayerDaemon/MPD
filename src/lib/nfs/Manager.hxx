/*
 * Copyright (C) 2003-2014 The Music Player Daemon Project
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

#ifndef MPD_NFS_MANAGER_HXX
#define MPD_NFS_MANAGER_HXX

#include "check.h"
#include "Connection.hxx"

#include <string>
#include <map>

/**
 * A manager for NFS connections.  Handles multiple connections to
 * multiple NFS servers.
 */
class NfsManager {
	class ManagedConnection final : public NfsConnection {
		NfsManager &manager;

	public:
		ManagedConnection(NfsManager &_manager, EventLoop &_loop,
				  const char *_server,
				  const char *_export_name)
			:NfsConnection(_loop, _server, _export_name),
			 manager(_manager) {}

	protected:
		/* virtual methods from NfsConnection */
		void OnNfsConnectionError(Error &&error) override;
	};

	EventLoop &loop;

	/**
	 * Maps server+":"+export_name (see method Key()) to
	 * #ManagedConnection.
	 */
	std::map<std::string, ManagedConnection> connections;

public:
	NfsManager(EventLoop &_loop)
		:loop(_loop) {}

	gcc_pure
	NfsConnection &GetConnection(const char *server,
				     const char *export_name);

private:
	gcc_pure
	static std::string Key(const char *server, const char *export_name) {
		return std::string(server) + ':' + export_name;
	}
};

#endif
