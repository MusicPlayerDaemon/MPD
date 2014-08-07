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

#include "config.h"
#include "ClientInternal.hxx"
#include "Partition.hxx"
#include "Instance.hxx"
#include "event/Loop.hxx"
#include "util/StringUtil.hxx"

#include <string.h>

BufferedSocket::InputResult
Client::OnSocketInput(void *data, size_t length)
{
	char *p = (char *)data;
	char *newline = (char *)memchr(p, '\n', length);
	if (newline == nullptr)
		return InputResult::MORE;

	TimeoutMonitor::ScheduleSeconds(client_timeout);

	BufferedSocket::ConsumeInput(newline + 1 - p);

	/* skip whitespace at the end of the line */
	char *end = StripRight(p, newline);

	/* terminate the string at the end of the line */
	*end = 0;

	CommandResult result = client_process_line(*this, p);
	switch (result) {
	case CommandResult::OK:
	case CommandResult::IDLE:
	case CommandResult::ERROR:
		break;

	case CommandResult::KILL:
		Close();
		partition.instance.event_loop->Break();
		return InputResult::CLOSED;

	case CommandResult::FINISH:
		if (Flush())
			Close();
		return InputResult::CLOSED;

	case CommandResult::CLOSE:
		Close();
		return InputResult::CLOSED;
	}

	if (IsExpired()) {
		Close();
		return InputResult::CLOSED;
	}

	return InputResult::AGAIN;
}
