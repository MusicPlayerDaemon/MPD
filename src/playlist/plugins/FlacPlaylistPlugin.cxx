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
#include "../SongEnumerator.hxx"
#include "song/DetachedSong.hxx"
#include "fs/Traits.hxx"
#include "fs/AllocatedPath.hxx"
#include "fs/NarrowPath.hxx"

#include <FLAC/metadata.h>

class FlacPlaylist final : public SongEnumerator {
	const std::string uri;

	FLAC__StreamMetadata *const cuesheet;
	const unsigned sample_rate;
	const FLAC__uint64 total_samples;

	unsigned next_track = 0;

public:
	FlacPlaylist(const char *_uri,
		     FLAC__StreamMetadata *_cuesheet,
		     const FLAC__StreamMetadata &streaminfo)
		:uri(_uri), cuesheet(_cuesheet),
		 sample_rate(streaminfo.data.stream_info.sample_rate),
		 total_samples(streaminfo.data.stream_info.total_samples) {
	}

	virtual ~FlacPlaylist() {
		FLAC__metadata_object_delete(cuesheet);
	}

	virtual std::unique_ptr<DetachedSong> NextSong() override;
};

std::unique_ptr<DetachedSong>
FlacPlaylist::NextSong()
{
	const FLAC__StreamMetadata_CueSheet &c = cuesheet->data.cue_sheet;

	/* find the next audio track */

	while (next_track < c.num_tracks &&
	       (c.tracks[next_track].number > c.num_tracks ||
		c.tracks[next_track].type != 0))
		++next_track;

	if (next_track >= c.num_tracks)
		return nullptr;

	FLAC__uint64 start = c.tracks[next_track].offset;
	++next_track;
	FLAC__uint64 end = next_track < c.num_tracks
		? c.tracks[next_track].offset
		: total_samples;

	auto song = std::make_unique<DetachedSong>(uri);
	song->SetStartTime(SongTime::FromScale(start, sample_rate));
	song->SetEndTime(SongTime::FromScale(end, sample_rate));
	return song;
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

	FLAC__StreamMetadata streaminfo;
	if (!FLAC__metadata_get_streaminfo(uri, &streaminfo) ||
	    streaminfo.data.stream_info.sample_rate == 0) {
		FLAC__metadata_object_delete(cuesheet);
		return nullptr;
	}

	return std::make_unique<FlacPlaylist>(uri, cuesheet, streaminfo);
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
