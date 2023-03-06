// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "MusicChunk.hxx"
#include "pcm/AudioFormat.hxx"
#include "tag/Tag.hxx"

#include <cassert>

MusicChunkInfo::MusicChunkInfo() noexcept = default;
MusicChunkInfo::~MusicChunkInfo() noexcept = default;

#ifndef NDEBUG
bool
MusicChunkInfo::CheckFormat(const AudioFormat other_format) const noexcept
{
	assert(other_format.IsValid());

	return length == 0 || audio_format == other_format;
}
#endif

std::span<std::byte>
MusicChunk::Write(const AudioFormat af,
		  SongTime data_time, uint16_t _bit_rate) noexcept
{
	assert(CheckFormat(af));
	assert(length == 0 || audio_format.IsValid());

	if (length == 0) {
		/* if the chunk is empty, nobody has set bitRate and
		   time yet */

		bit_rate = _bit_rate;
		time = data_time;

#ifndef NDEBUG
		audio_format = af;
#endif
	}

	const size_t frame_size = af.GetFrameSize();
	size_t num_frames = (sizeof(data) - length) / frame_size;
	return { data + length, num_frames * frame_size };
}

bool
MusicChunk::Expand(const AudioFormat af, size_t _length) noexcept
{
	const size_t frame_size = af.GetFrameSize();

	assert(length + _length <= sizeof(data));
	assert(audio_format == af);

	length += _length;

	return length + frame_size > sizeof(data);
}
