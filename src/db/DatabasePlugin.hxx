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

/** \file
 *
 * This header declares the db_plugin class.  It describes a
 * plugin API for databases of song metadata.
 */

#ifndef MPD_DATABASE_PLUGIN_HXX
#define MPD_DATABASE_PLUGIN_HXX

#include "Ptr.hxx"

struct ConfigBlock;
class EventLoop;
class DatabaseListener;

struct DatabasePlugin {
	/**
	 * This plugin requires a #Storage instance.  It contains only
	 * cached metadata from files in the #Storage.
	 */
	static constexpr unsigned FLAG_REQUIRE_STORAGE = 0x1;

	const char *name;

	unsigned flags;

	/**
	 * Allocates and configures a database.
	 *
	 * Throws #std::runtime_error on error.
	 *
	 * @param main_event_loop the #EventLoop running in the same
	 * thread which invokes #Database methods
	 * @param io_event_loop the #EventLoop running on the
	 * #EventThread, i.e. the one used for background I/O
	 */
	DatabasePtr (*create)(EventLoop &main_event_loop,
			      EventLoop &io_event_loop,
			      DatabaseListener &listener,
			      const ConfigBlock &block);

	constexpr bool RequireStorage() const {
		return flags & FLAG_REQUIRE_STORAGE;
	}
};

#endif
