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
