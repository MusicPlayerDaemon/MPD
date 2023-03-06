// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "Client.hxx"
#include "Config.hxx"
#include "Partition.hxx"
#include "Instance.hxx"
#include "util/StringStrip.hxx"

#include <cstring>

BufferedSocket::InputResult
Client::OnSocketInput(void *data, size_t length) noexcept
{
	if (background_command)
		return InputResult::PAUSE;

	char *p = (char *)data;
	char *newline = (char *)std::memchr(p, '\n', length);
	if (newline == nullptr)
		return InputResult::MORE;

	timeout_event.Schedule(client_timeout);

	BufferedSocket::ConsumeInput(newline + 1 - p);

	/* skip whitespace at the end of the line */
	char *end = StripRight(p, newline);

	/* terminate the string at the end of the line */
	*end = 0;

	CommandResult result = ProcessLine(p);
	switch (result) {
	case CommandResult::OK:
	case CommandResult::IDLE:
	case CommandResult::BACKGROUND:
	case CommandResult::ERROR:
		break;

	case CommandResult::KILL:
		partition->instance.Break();
		Close();
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
