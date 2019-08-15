/*
 * Copyright 2003-2019 The Music Player Daemon Project
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

/** \file
 *
 * Playlist plugin that reads embedded cue sheets from the "CUESHEET"
 * tag of a music file.
 */

#include "FlacPlaylistPlugin.hxx"
#include "../PlaylistPlugin.hxx"
#include "../MemorySongEnumerator.hxx"
#include "song/DetachedSong.hxx"
#include "fs/Traits.hxx"
#include "fs/AllocatedPath.hxx"
#include "fs/NarrowPath.hxx"
#include "util/ScopeExit.hxx"

#include <FLAC/metadata.h>

static auto
ToSongEnumerator(const char *uri,
		 const FLAC__StreamMetadata_CueSheet &c,
		 const unsigned sample_rate,
		 const FLAC__uint64 total_samples) noexcept
{
	std::forward_list<DetachedSong> songs;
	auto tail = songs.before_begin();

	for (unsigned i = 0; i < c.num_tracks; ++i) {
		const auto &track = c.tracks[i];
		if (track.type != 0)
			continue;

		const FLAC__uint64 start = track.offset;
		const FLAC__uint64 end = i + 1 < c.num_tracks
			? c.tracks[i + 1].offset
			: total_samples;

		tail = songs.emplace_after(tail, uri);
		auto &song = *tail;
		song.SetStartTime(SongTime::FromScale(start, sample_rate));
		song.SetEndTime(SongTime::FromScale(end, sample_rate));
	}

	return std::make_unique<MemorySongEnumerator>(std::move(songs));
}

static std::unique_ptr<SongEnumerator>
flac_playlist_open_uri(const char *uri,
		       gcc_unused Mutex &mutex)
{
	if (!PathTraitsUTF8::IsAbsolute(uri))
		/* only local files supported */
		return nullptr;

	const auto path_fs = AllocatedPath::FromUTF8Throw(uri);

	const NarrowPath narrow_path_fs(path_fs);

	FLAC__StreamMetadata *cuesheet;
	if (!FLAC__metadata_get_cuesheet(narrow_path_fs, &cuesheet))
		return nullptr;

	AtScopeExit(cuesheet) { FLAC__metadata_object_delete(cuesheet); };

	FLAC__StreamMetadata streaminfo;
	if (!FLAC__metadata_get_streaminfo(narrow_path_fs, &streaminfo) ||
	    streaminfo.data.stream_info.sample_rate == 0) {
		return nullptr;
	}

	const unsigned sample_rate = streaminfo.data.stream_info.sample_rate;
	const FLAC__uint64 total_samples = streaminfo.data.stream_info.total_samples;

	return ToSongEnumerator(uri, cuesheet->data.cue_sheet,
				sample_rate, total_samples);
}

static const char *const flac_playlist_suffixes[] = {
	"flac",
	nullptr
};

const struct playlist_plugin flac_playlist_plugin = {
	"flac",

	nullptr,
	nullptr,
	flac_playlist_open_uri,
	nullptr,

	nullptr,
	flac_playlist_suffixes,
	nullptr,
};
