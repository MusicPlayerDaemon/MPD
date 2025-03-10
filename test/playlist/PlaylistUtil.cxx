// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "PlaylistUtil.hxx"
#include "playlist/PlaylistRegistry.hxx"
#include "playlist/SongEnumerator.hxx"
#include "song/DetachedSong.hxx"
#include "input/MemoryInputStream.hxx"
#include "input/Ptr.hxx"
#include "thread/Mutex.hxx"
#include "io/BufferedOutputStream.hxx"
#include "io/StringOutputStream.hxx"
#include "util/SpanCast.hxx"
#include "SongSave.hxx"

std::unique_ptr<SongEnumerator>
ParsePlaylist(const char *uri, std::string_view contents)
{
	Mutex mutex;
	InputStreamPtr input{new MemoryInputStream{uri, mutex, AsBytes(contents)}};
	return playlist_list_open_stream(std::move(input), uri);
}

std::string
ToString(SongEnumerator &e)
{
	StringOutputStream sos;

	WithBufferedOutputStream(sos, [&e](auto &bos){
		while (const auto song = e.NextSong()) {
			bos.Write('\n');
			song_save(bos, *song);
		}

		bos.Write('\n');
	});

	return std::move(sos).GetValue();
}
