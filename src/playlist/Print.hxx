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

#ifndef MPD_PLAYLIST__PRINT_HXX
#define MPD_PLAYLIST__PRINT_HXX

class Client;

/**
 * Send the playlist file to the client.
 *
 * @param client the client which requested the playlist
 * @param uri the URI of the playlist file in UTF-8 encoding
 * @param detail true if all details should be printed
 * @return true on success, false if the playlist does not exist
 */
bool
playlist_file_print(Client &client, const char *uri, bool detail);

#endif
