/*
 * Copyright (C) 2003-2015 The Music Player Daemon Project
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

#ifndef MPD_RESPONSE_HXX
#define MPD_RESPONSE_HXX

#include "check.h"
#include "protocol/Ack.hxx"

#include <stddef.h>
#include <stdarg.h>

class Client;

class Response {
	Client &client;

public:
	explicit Response(Client &_client):client(_client) {}

	Response(const Response &) = delete;
	Response &operator=(const Response &) = delete;

	bool Write(const void *data, size_t length);
	bool Write(const char *data);
	bool FormatV(const char *fmt, va_list args);
	bool Format(const char *fmt, ...);

	void Error(enum ack code, const char *msg);
	void FormatError(enum ack code, const char *fmt, ...);
};

#endif
