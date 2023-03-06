// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_DB_CONFIG_HXX
#define MPD_DB_CONFIG_HXX

#include "Ptr.hxx"

struct ConfigData;
class EventLoop;
class DatabaseListener;

/**
 * Read database configuration settings and create a #Database
 * instance from it, but do not open it.  Returns nullptr if no
 * database is configured.
 *
 * Throws #std::runtime_error on error.
 */
DatabasePtr
CreateConfiguredDatabase(const ConfigData &config,
			 EventLoop &main_event_loop, EventLoop &io_event_loop,
			 DatabaseListener &listener);

#endif
