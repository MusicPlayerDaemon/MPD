/*
 * Copyright 2003-2019 The Music Player Daemon Project
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

#include "Client.hxx"
#include "Config.hxx"
#include "Partition.hxx"
#include "Instance.hxx"
#include "BackgroundCommand.hxx"
#include "config.h"

Client::~Client() noexcept
{
	if (FullyBufferedSocket::IsDefined())
		FullyBufferedSocket::Close();

	if (background_command) {
		background_command->Cancel();
		background_command.reset();
	}
}

void
Client::SetBackgroundCommand(std::unique_ptr<BackgroundCommand> _bc) noexcept
{
	assert(!background_command);
	assert(_bc);

	background_command = std::move(_bc);

	/* disable timeouts while in "idle" */
	timeout_event.Cancel();
}

void
Client::OnBackgroundCommandFinished() noexcept
{
	assert(background_command);

	background_command.reset();

	/* just in case OnSocketInput() has returned
	   InputResult::PAUSE meanwhile */
	ResumeInput();

	timeout_event.Schedule(client_timeout);
}

Instance &
Client::GetInstance() noexcept
{
	return partition->instance;
}

playlist &
Client::GetPlaylist() noexcept
{
	return partition->playlist;
}

PlayerControl &
Client::GetPlayerControl() noexcept
{
	return partition->pc;
}

#ifdef ENABLE_DATABASE

const Database *
Client::GetDatabase() const noexcept
{
	return partition->instance.GetDatabase();
}

const Database &
Client::GetDatabaseOrThrow() const
{
	return partition->instance.GetDatabaseOrThrow();
}

const Storage *
Client::GetStorage() const noexcept
{
	return partition->instance.storage;
}

#endif
