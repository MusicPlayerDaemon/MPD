/*
 * Copyright 2003-2018 The Music Player Daemon Project
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

#ifndef MPD_REMOTE_ALBUM_HANDLER_HXX
#define MPD_REMOTE_ALBUM_HANDLER_HXX

#include "check.h"

#include <string>
#include <mutex>
#include <condition_variable>

class QobuzAlbumHandler;
class QobuzAlbumRequest;

class RemoteAlbumHandler final: public QobuzAlbumHandler {
	std::mutex mutex;
	std::condition_variable cond;
	bool done = false;
	std::unique_ptr<QobuzAlbumRequest> album_request;

public:
	RemoteAlbumHandler(const std::string &uri) noexcept;
	~RemoteAlbumHandler() noexcept;
	void Join();

private:
	void OnQobuzAlbumSuccess(const Album &album) noexcept;
	void OnQobuzAlbumError(std::exception_ptr error) noexcept;
};

#endif
