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
#include "QobuzQueue.hxx"
#include "QobuzModel.hxx"
#include "QobuzInputPlugin.hxx"
#include "QobuzClient.hxx"
#include "util/RuntimeError.hxx"
#include "util/StringCompare.hxx"
#include "protocol/ArgParser.hxx"
#include "client/Client.hxx"
#include "DetachedSong.hxx"
#include "Partition.hxx"
#include "tag/Builder.hxx"
#include "tag/Tag.hxx"
#include "queue/Playlist.hxx"
#include "player/Control.hxx"
#include "BulkEdit.hxx"

#include <chrono>
#include <string>

static std::string
MakeAlbumUrl(QobuzClient &client, const char *album_id, const RangeArg &range)
{
	std::string offset = std::to_string(range.start);
	auto limit_int = range.end - range.start;
	limit_int = std::min(1000U, limit_int);
	std::string limit = std::to_string(limit_int);
	return client.MakeSignedUrl("album", "get",
				    {
					    {"app_id", client.GetSession().app_id.c_str()},
					    {"album_id", album_id},
					    {"offset", offset.c_str()},
					    {"limit", limit.c_str()},
				    });
}

static std::string
MakePlaylistUrl(QobuzClient &client, const char *playlist_id, const RangeArg &range)
{
	std::string offset = std::to_string(range.start);
	auto limit_int = range.end - range.start;
	limit_int = std::min(1000U, limit_int);
	std::string limit = std::to_string(limit_int);
	return client.MakeSignedUrl("playlist", "get",
				    {
					    {"app_id", client.GetSession().app_id.c_str()},
					    {"playlist_id", playlist_id},
					    {"extra", "tracks"},
					    {"offset", offset.c_str()},
					    {"limit", limit.c_str()},
				    });
}

QobuzQueue::QobuzQueue()
{

}

QobuzQueue::~QobuzQueue()
{

}

bool
QobuzQueue::Add(Client &client, const char *uri, const RangeArg &range)
{
	const char *type = StringAfterPrefix(uri, "qobuz://");
	if (type == nullptr || *type == 0) {
		return false;
	}

	const char *album_id = StringAfterPrefix(type, "album/");
	if (album_id != nullptr && *album_id != 0) {
		Album album;
		auto &qobuz = GetQobuzClient();
		QobuzRequest<Album> request(qobuz, album, MakeAlbumUrl(qobuz, album_id, range).c_str(), *this);
		std::unique_lock<std::mutex> locker(mutex);
		exception_ptr = nullptr;
		request.Start();
		if (cond.wait_for(locker, std::chrono::seconds(DEFAULT_TIMEOUT)) ==  std::cv_status::timeout) {
			throw std::runtime_error("Add qobuz album timeout");
		}
		if (exception_ptr) {
			std::rethrow_exception(exception_ptr);
		}
		auto &partition = client.GetPartition();
		const ScopeBulkEdit bulk_edit(partition);
		for (const auto &track : album.tracks.items) {
			auto song_uri = std::string("qobuz://track/") + std::to_string(track.id);
			DetachedSong song(song_uri);
			TagBuilder builder;
			if (!track.title.empty())
				builder.AddItem(TAG_TITLE, track.title.c_str());
			if (!album.title.empty())
				builder.AddItem(TAG_ALBUM, album.title.c_str());
			if (!album.artist.name.empty())
				builder.AddItem(TAG_ALBUM_ARTIST, album.artist.name.c_str());
			if (!track.performer.name.empty())
				builder.AddItem(TAG_PERFORMER, track.performer.name.c_str());
			if (track.duration > 0)
				builder.SetDuration(SignedSongTime::FromS(track.duration));
			if (!album.image.large.empty())
				builder.AddItem(TAG_ALBUM_URI, album.image.large.c_str());
			if (!album.genre.name.empty())
				builder.AddItem(TAG_GENRE, album.genre.name.c_str());
			song.SetTag(std::move(builder.Commit()));
			partition.playlist.AppendSong(partition.pc, std::move(song));
		}
		return true;
	}
	const char *playlist_id = StringAfterPrefix(type, "playlist/");
	if (playlist_id != nullptr && *playlist_id != 0) {
		Playlist playlist;
		auto &qobuz = GetQobuzClient();
		QobuzRequest<Playlist> request(qobuz, playlist, MakePlaylistUrl(qobuz, playlist_id, range).c_str(), *this);
		std::unique_lock<std::mutex> locker(mutex);
		exception_ptr = nullptr;
		request.Start();
		if (cond.wait_for(locker, std::chrono::seconds(DEFAULT_TIMEOUT)) ==  std::cv_status::timeout) {
			throw std::runtime_error("Add qobuz playlist timeout");
		}
		if (exception_ptr) {
			std::rethrow_exception(exception_ptr);
		}
		auto &partition = client.GetPartition();
		const ScopeBulkEdit bulk_edit(partition);
		for (const auto &track : playlist.tracks.items) {
			auto song_uri = std::string("qobuz://track/") + std::to_string(track.id);
			DetachedSong song(song_uri);
			TagBuilder builder;
			if (!track.title.empty())
				builder.AddItem(TAG_TITLE, track.title.c_str());
			if (!track.album.title.empty())
				builder.AddItem(TAG_ALBUM, track.album.title.c_str());
			if (!track.album.artist.name.empty())
				builder.AddItem(TAG_ALBUM_ARTIST, track.album.artist.name.c_str());
			if (!track.performer.name.empty())
				builder.AddItem(TAG_PERFORMER, track.performer.name.c_str());
			if (track.duration > 0)
				builder.SetDuration(SignedSongTime::FromS(track.duration));
			if (!track.album.image.large.empty())
				builder.AddItem(TAG_ALBUM_URI, track.album.image.large.c_str());
			if (!track.album.genre.name.empty())
				builder.AddItem(TAG_GENRE, track.album.genre.name.c_str());
			song.SetTag(std::move(builder.Commit()));
			partition.playlist.AppendSong(partition.pc, std::move(song));
		}
		return true;
	}

	return false;
}

void
QobuzQueue::OnQobuzSuccess() noexcept
{
	std::lock_guard<std::mutex> locker(mutex);
	cond.notify_all();
}

void
QobuzQueue::OnQobuzError(std::exception_ptr error) noexcept
{
	std::lock_guard<std::mutex> locker(mutex);
	exception_ptr = error;
	cond.notify_all();
}
