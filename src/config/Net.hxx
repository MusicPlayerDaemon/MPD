// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

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
