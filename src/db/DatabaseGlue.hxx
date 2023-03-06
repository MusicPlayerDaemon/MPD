// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_DATABASE_GLUE_HXX
#define MPD_DATABASE_GLUE_HXX

#include "Ptr.hxx"

struct ConfigBlock;
class EventLoop;
class DatabaseListener;

/**
 * Initialize the database library.
 *
 * Throws #std::runtime_error on error.
 *
 * @param block the database configuration block
 */
DatabasePtr
DatabaseGlobalInit(EventLoop &main_event_loop,
		   EventLoop &io_event_loop,
		   DatabaseListener &listener,
		   const ConfigBlock &block);

#endif
