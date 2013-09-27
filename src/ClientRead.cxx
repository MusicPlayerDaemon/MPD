/*
 * Copyright (C) 2003-2013 The Music Player Daemon Project
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

#include "config.h"
#include "ClientInternal.hxx"
#include "Main.hxx"
#include "event/Loop.hxx"

#include <glib.h>

#include <assert.h>
#include <string.h>

BufferedSocket::InputResult
Client::OnSocketInput(const void *data, size_t length)
{
	const char *p = (const char *)data;
	const char *newline = (const char *)memchr(p, '\n', length);
	if (newline == NULL)
		return InputResult::MORE;

	TimeoutMonitor::ScheduleSeconds(client_timeout);

	char *line = g_strndup(p, newline - p);
	BufferedSocket::ConsumeInput(newline + 1 - p);

	enum command_return result = client_process_line(this, line);
	g_free(line);

	switch (result) {
	case COMMAND_RETURN_OK:
	case COMMAND_RETURN_IDLE:
	case COMMAND_RETURN_ERROR:
		break;

	case COMMAND_RETURN_KILL:
		Close();
		main_loop->Break();
		return InputResult::CLOSED;

	case COMMAND_RETURN_CLOSE:
		Close();
		return InputResult::CLOSED;
	}

	if (IsExpired()) {
		Close();
		return InputResult::CLOSED;
	}

	return InputResult::AGAIN;
}
