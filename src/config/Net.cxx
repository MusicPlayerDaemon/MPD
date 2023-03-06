// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

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
