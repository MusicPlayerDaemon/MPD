/*
 * Copyright 2015-2018 Cary Audio
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

#include "config.h"
#include "CurlSocket.hxx"
#include "Log.hxx"
#include "util/StringUtil.hxx"
#include "util/StringCompare.hxx"
#include "util/Domain.hxx"

#include <stdio.h>
#include <strings.h>
#include <stdlib.h>
#include <pthread.h>
#include <curl/curl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>

//#define ENABLE_CURL_DEBUG

static constexpr Domain domain("curl_socket");

static unsigned default_timeout = 60*1000; // ms

CurlSocket::CurlSocket()
{

}

CurlRespond
CurlSocket::request(const CurlCommand &cmd)
{
	struct sockaddr_in client_addr;
	struct sockaddr_in server_addr;
	int res;
	int client_socket;
	fd_set fdr;
	struct timeval timeout;

	if (!cmd.isDefined()) {
		throw HttpError::unexpectedError("unkown cmd: %s", cmd.command_cstr());
	}

	bzero(&client_addr,sizeof(client_addr));
	client_addr.sin_family = AF_INET;
	client_addr.sin_addr.s_addr = htons(INADDR_ANY);
	client_addr.sin_port = htons(0);
	client_socket = socket(AF_INET, SOCK_STREAM, 0);
	if( client_socket < 0) {
		close(client_socket);
		throw HttpError::unexpectedError("Create Socket Failed!");
	}
	bzero(&server_addr,sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	if(inet_aton("127.0.0.1",&server_addr.sin_addr) == 0) {
		close(client_socket);
		throw HttpError::unexpectedError("Server IP Address Error!");
	}
	server_addr.sin_port = htons(CURL_SOCKET_PORT);
	if(connect(client_socket, (struct sockaddr*)&server_addr, sizeof(server_addr))) {
		close(client_socket);
		throw HttpError::unexpectedError("system socket server error!");
	}
	std::string cmdstr = cmd.buildCommandString();
	FormatDebug(domain, "tx: %s", cmdstr.c_str());
	send(client_socket, cmdstr.c_str(), cmdstr.size(), 0);

	FD_ZERO(&fdr);
	FD_SET(client_socket, &fdr);
	timeout.tv_sec = default_timeout / 1000;
	timeout.tv_usec = (default_timeout % 1000) * 1000;
	res = select(client_socket + 1, &fdr, nullptr, nullptr, &timeout);
	std::string out;
	if (res > 0) {
		if (FD_ISSET(client_socket, &fdr)) {
			while (true) {
				char buffer[BUFFER_SIZE];
				bzero(buffer, sizeof(buffer));
				size_t len = recv(client_socket, buffer, sizeof(buffer), 0);
				out.append(buffer, len);
				if (len == 0) {
					break;
				}
			}
			//fprintf(stderr, "len=%d rx: %s\n", out.size(), out.c_str());
		} else {
			close(client_socket);
			throw HttpError::unexpectedError("");
		}
	} else if (res < 0) {
		close(client_socket);
		throw HttpError::unexpectedError("socket error!");
	} else {
		close(client_socket);
		throw HttpError::requestTimeout();
	}

	close(client_socket);

	if (out.empty()) {
		throw HttpError::unexpectedError("no return!");
	}

	CurlRespond respond;
	std::string message;
	std::size_t found = out.find('\n');
	while (found != std::string::npos) {
		const char *str = out.c_str();
		if (StringStartsWith(str, "ack: ")) {
			respond.ack = strtol(str + 5, nullptr, 10);
		} else if (StringStartsWith(str, "message: ")) {
			message = out.substr(9, found-9);
		} else if (StringStartsWith(str, "etag: ")) {
			respond.etag = out.substr(6, found-6);
			if (strcasecmp(respond.etag.c_str(), "W/") == 0) {
				respond.etag.erase(0, 2);
				FormatDebug(domain, "==etag: %s\n", respond.etag.c_str());
			}
		} else if (StringStartsWith(str, "data: ")) {
			respond.rxdata = out.substr(6);
			out.erase(0);
			break;
		}
		out.erase(0, found+1);
		found = out.find('\n');
	}
	if (StringStartsWith(out.c_str(), "data: ")) {
		respond.rxdata = out.substr(6);
	}
	FormatDebug(domain, "ack: %d\n", respond.ack);
	FormatDebug(domain, "message: %s\n", message.c_str());
	FormatDebug(domain, "etag: %s\n", respond.etag.c_str());
	FormatDebug(domain, "data: %s\n", respond.rxdata.c_str());

	if (respond.ack == 52) {
		throw HttpError::unexpectedError(message.c_str());
	}

	return respond;
}
