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

/** \file
 *
 * Playlist plugin that reads embedded cue sheets from the "CUESHEET"
 * tag of a music file.
 */

#include "FlacPlaylistPlugin.hxx"
#include "../PlaylistPlugin.hxx"
#include "../MemorySongEnumerator.hxx"
#include "lib/xiph/FlacMetadataChain.hxx"
#include "lib/xiph/FlacMetadataIterator.hxx"
#include "song/DetachedSong.hxx"
#include "input/InputStream.hxx"
#include "util/RuntimeError.hxx"

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
flac_playlist_open_stream(InputStreamPtr &&is)
{
	FlacMetadataChain chain;
	if (!chain.Read(*is))
		throw FormatRuntimeError("Failed to read FLAC metadata: %s",
					 chain.GetStatusString());

	FlacMetadataIterator iterator((FLAC__Metadata_Chain *)chain);

	unsigned sample_rate = 0;
	FLAC__uint64 total_samples;

	do {
		auto &block = *iterator.GetBlock();
		switch (block.type) {
		case FLAC__METADATA_TYPE_STREAMINFO:
			sample_rate = block.data.stream_info.sample_rate;
			total_samples = block.data.stream_info.total_samples;
			break;

		case FLAC__METADATA_TYPE_CUESHEET:
			if (sample_rate == 0)
				break;

			return ToSongEnumerator("", block.data.cue_sheet,
						sample_rate, total_samples);

		default:
			break;
		}
	} while (iterator.Next());

	return nullptr;
}

static constexpr const char *flac_playlist_suffixes[] = {
	"flac",
	nullptr
};

const PlaylistPlugin flac_playlist_plugin =
	PlaylistPlugin("flac", flac_playlist_open_stream)
	.WithSuffixes(flac_playlist_suffixes);
