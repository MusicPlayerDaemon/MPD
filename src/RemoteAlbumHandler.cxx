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

#include "config.h"
#include "RemoteAlbumHandler.hxx"
#include "input/plugins/QobuzAlbumRequest.hxx"
#include "input/plugins/QobuzInputPlugin.hxx"

gcc_pure
static const char *
ExtractQobuzAlbumId(const char *uri)
{
	// TODO: what's the standard "qobuz://" URI syntax?
	const char *album_id = StringAfterPrefix(uri, "qobuz://album/");
	if (album_id == nullptr)
		return nullptr;

	if (*album_id == 0)
		return nullptr;

	return album_id;
}

RemoteAlbumHandler::RemoteAlbumHandler(const std::string &uri) noexcept
{
	const char *album_id = ExtractQobuzAlbumId(uri);
	if (!album_id) {
		return;
	}
	auto &client = GetQobuzClient();
	album_request = std::make_unique<QobuzAlbumRequest>(client, client.GetSession(),
		   album_id, *this);
	album_request->Start();
}

RemoteAlbumHandler::~RemoteAlbumHandler() noexcept
{
}

void
RemoteAlbumHandler::Join()
{
	std::unique_lock<std::mutex> locker(mutex);
	if (!album_request) {
		return;
	}
	if(!done) {
		cond.wait(locker);
	}
	album_request.reset();
}

void
RemoteAlbumHandler::OnQobuzAlbumSuccess(const Album &album) noexcept
{
	std::lock_guard<std::mutex> locker(mutex);
	done = true;
	cond.notify_all();
}

void
RemoteAlbumHandler::OnQobuzAlbumError(std::exception_ptr error) noexcept
{
	std::lock_guard<std::mutex> locker(mutex);
	done = true;
	cond.notify_all();
}
