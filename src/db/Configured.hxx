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
