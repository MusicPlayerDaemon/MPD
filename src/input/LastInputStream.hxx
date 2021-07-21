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

#ifndef MPD_LAST_INPUT_STREAM_HXX
#define MPD_LAST_INPUT_STREAM_HXX

#include "Ptr.hxx"
#include "thread/Mutex.hxx"
#include "event/TimerEvent.hxx"

#include <string>

/**
 * A helper class which maintains an #InputStream that is opened once
 * and may be reused later for some time.  It will be closed
 * automatically after some time.
 *
 * This class is not thread-safe.  All methods must be called on the
 * thread which runs the #EventLoop.
 */
class LastInputStream {
	std::string uri;

	Mutex mutex;

	InputStreamPtr is;

	TimerEvent close_timer;

public:
	explicit LastInputStream(EventLoop &event_loop) noexcept;
	~LastInputStream() noexcept;

	/**
	 * Open an #InputStream instance with the given opener
	 * function, but returns the cached instance if it matches.
	 *
	 * This object keeps owning the #InputStream; the caller shall
	 * not close it.
	 */
	template<typename U, typename O>
	InputStream *Open(U &&new_uri, O &&open) {
		if (new_uri == uri) {
			if (is)
				/* refresh the timeout */
				ScheduleClose();

			return is.get();
		}

		Close();

		is = open(new_uri, mutex);
		uri = std::forward<U>(new_uri);
		if (is)
			ScheduleClose();
		return is.get();
	}

	void Close() noexcept;

private:
	void ScheduleClose() noexcept {
		close_timer.Schedule(std::chrono::seconds(20));
	}

	void OnCloseTimer() noexcept;
};

#endif
