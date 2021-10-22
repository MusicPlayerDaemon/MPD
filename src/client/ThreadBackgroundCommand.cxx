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

#include "ThreadBackgroundCommand.hxx"
#include "Client.hxx"
#include "Response.hxx"
#include "command/CommandError.hxx"

ThreadBackgroundCommand::ThreadBackgroundCommand(Client &_client) noexcept
	:thread(BIND_THIS_METHOD(_Run)),
	 defer_finish(_client.GetEventLoop(), BIND_THIS_METHOD(DeferredFinish)),
	 client(_client)
{
}

void
ThreadBackgroundCommand::_Run() noexcept
{
	assert(!error);

	try {
		Run();
	} catch (...) {
		error = std::current_exception();
	}

	defer_finish.Schedule();
}

void
ThreadBackgroundCommand::DeferredFinish() noexcept
{
	/* free the Thread */
	thread.Join();

	/* send the response */
	Response response(client, 0);

	if (error) {
		PrintError(response, error);
	} else {
		SendResponse(response);
		client.WriteOK();
	}

	/* delete this object */
	client.OnBackgroundCommandFinished();
}

void
ThreadBackgroundCommand::Cancel() noexcept
{
	CancelThread();
	thread.Join();

	/* cancel the InjectEvent, just in case the Thread has
	   meanwhile finished execution */
	defer_finish.Cancel();
}
