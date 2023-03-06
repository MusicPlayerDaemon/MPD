// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_THREAD_BACKGROUND_COMMAND_HXX
#define MPD_THREAD_BACKGROUND_COMMAND_HXX

#include "BackgroundCommand.hxx"
#include "event/InjectEvent.hxx"
#include "thread/Thread.hxx"

#include <exception>

class Client;
class Response;

/**
 * A #BackgroundCommand which defers execution into a new thread.
 */
class ThreadBackgroundCommand : public BackgroundCommand {
	Thread thread;
	InjectEvent defer_finish;
	Client &client;

	/**
	 * The error thrown by Run().
	 */
	std::exception_ptr error;

public:
	explicit ThreadBackgroundCommand(Client &_client) noexcept;

	auto &GetEventLoop() const noexcept {
		return defer_finish.GetEventLoop();
	}

	void Start() {
		thread.Start();
	}

	void Cancel() noexcept final;

private:
	void _Run() noexcept;
	void DeferredFinish() noexcept;

protected:
	/**
	 * If this method throws, the exception will be converted to a
	 * MPD response, and SendResponse() will not be called.
	 */
	virtual void Run() = 0;

	/**
	 * Send the response after Run() has finished.  Note that you
	 * must not send errors here; if an error occurs, Run() should
	 * throw an exception instead.
	 */
	virtual void SendResponse(Response &response) noexcept = 0;

	virtual void CancelThread() noexcept = 0;
};

#endif
