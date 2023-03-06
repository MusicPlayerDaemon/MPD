// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

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
