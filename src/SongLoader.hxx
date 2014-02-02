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

#ifndef MPD_SONG_LOADER_HXX
#define MPD_SONG_LOADER_HXX

#include "Compiler.h"

class Client;
class DetachedSong;
class Error;

/**
 * A utility class that loads a #DetachedSong object by its URI.  If
 * the URI is an absolute local file, it applies security checks via
 * Client::AllowFile().  If no #Client pointer was specified, then it
 * is assumed that all local files are allowed.
 */
class SongLoader {
	const Client *const client;

public:
	explicit SongLoader(const Client &_client)
		:client(&_client) {}
	explicit SongLoader(const Client *_client)
		:client(_client) {}

	gcc_nonnull_all
	DetachedSong *LoadSong(const char *uri_utf8, Error &error) const;

private:
	gcc_nonnull_all
	DetachedSong *LoadFile(const char *path_utf8, Error &error) const;
};

#endif
