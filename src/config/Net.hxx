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

#ifndef MPD_CONFIG_NET_HXX
#define MPD_CONFIG_NET_HXX

class ServerSocket;

/**
 * Sets the address or local socket of a ServerSocket instance
 * There are three possible ways
 * 1) Set address to a valid ip address and specify port.
 *    server_socket will listen on this address/port tuple.
 * 2) Set address to null and specify port.
 *    server_socket will listen on ANY address on that port.
 * 3) Set address to a path of a local socket. port is ignored.
 *    server_socket will listen on this local socket.
 *
 * Throws #std::runtime_error on error.
 *
 * @param server_socket the instance to modify
 * @param address the address to listen on
 * @param port the port to listen on
 */
void
ServerSocketAddGeneric(ServerSocket &server_socket, const char *address, unsigned int port);

#endif
