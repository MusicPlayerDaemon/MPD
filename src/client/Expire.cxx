// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "Client.hxx"
#include "BackgroundCommand.hxx"
#include "Domain.hxx"
#include "Log.hxx"

void
Client::SetExpired() noexcept
{
	if (IsExpired())
		return;

	if (background_command) {
		background_command->Cancel();
		background_command.reset();
	}

	FullyBufferedSocket::Close();
	timeout_event.Schedule(Event::Duration::zero());
}

void
Client::OnTimeout() noexcept
{
	if (!IsExpired()) {
		assert(!idle_waiting);
		assert(!background_command);

		FmtDebug(client_domain, "[{}] timeout", num);
	}

	Close();
}
