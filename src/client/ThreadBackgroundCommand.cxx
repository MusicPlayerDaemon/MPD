// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

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
