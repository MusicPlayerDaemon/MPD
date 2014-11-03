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
#include "Compiler.h"

#include <boost/intrusive/set.hpp>

/**
 * A manager for NFS connections.  Handles multiple connections to
 * multiple NFS servers.
 */
class NfsManager {
	struct LookupKey {
		const char *server;
		const char *export_name;
	};

	class ManagedConnection final
		: public NfsConnection,
		  public boost::intrusive::set_base_hook<boost::intrusive::link_mode<boost::intrusive::normal_link>> {
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

	struct Compare {
		gcc_pure
		bool operator()(const LookupKey a,
				const ManagedConnection &b) const;

		gcc_pure
		bool operator()(const ManagedConnection &a,
				const LookupKey b) const;
	};

	EventLoop &loop;

	/**
	 * Maps server and export_name to #ManagedConnection.
	 */
	typedef boost::intrusive::set<ManagedConnection,
				      boost::intrusive::compare<Compare>,
				      boost::intrusive::constant_time_size<false>> Map;

	Map connections;

public:
	NfsManager(EventLoop &_loop)
		:loop(_loop) {}

	/**
	 * Must be run from EventLoop's thread.
	 */
	~NfsManager();

	gcc_pure
	NfsConnection &GetConnection(const char *server,
				     const char *export_name);
};

#endif
