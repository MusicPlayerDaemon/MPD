/*
 * Copyright 2003-2021 The Music Player Daemon Project
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
#include "IdleFlags.hxx"
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

void
Client::SetPartition(Partition &new_partition) noexcept
{
	if (partition == &new_partition)
		return;

	partition->clients.erase(partition->clients.iterator_to(*this));
	partition = &new_partition;
	partition->clients.push_back(*this);

	/* set idle flags for those subsystems which are specific to
	   the current partition to force the client to reload its
	   state */
	idle_flags |= IDLE_PLAYLIST|IDLE_PLAYER|IDLE_MIXER|IDLE_OUTPUT|IDLE_OPTIONS;
	/* note: we're not using IdleAdd() here because we don't need
	   to notify the client; the method is only used while this
	   client's "partition" command is handled, which means the
	   client is currently active and doesn't need to be woken
	   up */
}

Instance &
Client::GetInstance() const noexcept
{
	return partition->instance;
}

playlist &
Client::GetPlaylist() const noexcept
{
	return partition->playlist;
}

PlayerControl &
Client::GetPlayerControl() const noexcept
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
