// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_BACKGROUND_COMMAND_HXX
#define MPD_BACKGROUND_COMMAND_HXX

/**
 * A command running in background.  It can take some time to finish,
 * and will then call Client::OnBackgroundCommandFinished() from
 * inside the client's #EventLoop thread.  The important point is that
 * such a long-running command does not block MPD's main loop, and
 * other clients can still be handled meanwhile.
 *
 * (Note: "idle" is not a "background command" by this definition; it
 * is a special case.)
 *
 * @see ThreadBackgroundCommand
 */
class BackgroundCommand {
public:
	virtual ~BackgroundCommand() = default;

	/**
	 * Cancel command execution.  After this method returns, the
	 * object will be deleted.  It will be called from the
	 * #Client's #EventLoop thread.
	 */
	virtual void Cancel() noexcept = 0;
};

#endif
