// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "Client.hxx"

#include <string.h>

bool
Client::Write(const void *data, size_t length) noexcept
{
	/* if the client is going to be closed, do nothing */
	return !IsExpired() && FullyBufferedSocket::Write(data, length);
}
