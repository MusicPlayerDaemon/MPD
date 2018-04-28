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

#pragma once

#include <string>

#define CURL_SOCKET_PORT	6276
#define BUFFER_SIZE 4096 * 10
#define LENGTH_OF_LISTEN_QUEUE 100
#define NEWLINE		"\n"

struct CurlRespond
{
	CurlRespond(): done(false), ack(0) {}

	bool done = false;

	int ack = 0;

	std::string etag;

	size_t rxlen = 0;

	std::string rxdata;
};

struct CurlCommand
{
	enum Command { UNKOWN, POST, GET, DELETE, PUT };

	Command command;

	std::string url;

	std::string data;

	std::string etag;

	std::string authorization;

	std::string content_type;

	CurlCommand(Command _cmd,
		const std::string &_url,
		const std::string &_data,
		const std::string &_etag)
		: command(_cmd), url(_url), data(_data), etag(_etag) {}

	CurlCommand(Command _cmd,
		const std::string &_url,
		const std::string &_data)
		: command(_cmd), url(_url), data(_data) {}

	bool isDefined() const;

	bool isPost() const {
		return command == POST;
	}

	bool isGet() const {
		return command == GET;
	}

	bool isDelete() const {
		return command == DELETE;
	}

	bool isPut() const {
		return command == PUT;
	}

	std::string buildCommandString() const;

	const char *command_cstr() const;
};
