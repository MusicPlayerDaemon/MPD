// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

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

Storage *
Client::GetStorage() const noexcept
{
	return partition->instance.storage;
}

#endif
