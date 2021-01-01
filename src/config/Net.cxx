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

#include "Net.hxx"
#include "event/ServerSocket.hxx"
#include "Path.hxx"
#include "fs/AllocatedPath.hxx"

void
ServerSocketAddGeneric(ServerSocket &server_socket, const char *address, unsigned int port)
{
	if (address == nullptr || 0 == strcmp(address, "any")) {
		server_socket.AddPort(port);
	} else if (address[0] == '/' || address[0] == '~') {
		server_socket.AddPath(ParsePath(address));
	} else if (address[0] == '@') {
		server_socket.AddAbstract(address);
	} else {
		server_socket.AddHost(address, port);
	}
}
