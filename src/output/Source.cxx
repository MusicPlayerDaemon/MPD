// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "Source.hxx"
#include "MusicChunk.hxx"
#include "filter/Filter.hxx"
#include "filter/Prepared.hxx"
#include "filter/plugins/ReplayGainFilterPlugin.hxx"
#include "pcm/Mix.hxx"
#include "lib/fmt/AudioFormatFormatter.hxx"
#include "lib/fmt/RuntimeError.hxx"
#include "thread/Mutex.hxx"

#include <string.h>

AudioOutputSource::AudioOutputSource() noexcept = default;
AudioOutputSource::~AudioOutputSource() noexcept = default;

AudioFormat
AudioOutputSource::Open(const AudioFormat audio_format, const MusicPipe &_pipe,
			PreparedFilter *prepared_replay_gain_filter,
			PreparedFilter *prepared_other_replay_gain_filter,
			PreparedFilter &prepared_filter)
{
	assert(audio_format.IsValid());

	if (!IsOpen() || &_pipe != &pipe.GetPipe()) {
		current_chunk = nullptr;
		pipe.Init(_pipe);
	}

	/* (re)open the filter */

	if (filter && (filter_flushed || audio_format != in_audio_format))
		/* the filter must be reopened on all input format
		   changes */
		CloseFilter();

	if (filter == nullptr)
		/* open the filter */
		OpenFilter(audio_format,
			   prepared_replay_gain_filter,
			   prepared_other_replay_gain_filter,
			   prepared_filter);

	in_audio_format = audio_format;
	return filter->GetOutAudioFormat();
}

void
AudioOutputSource::Close() noexcept
{
	assert(in_audio_format.IsValid());
	in_audio_format.Clear();

	CloseFilter();

	Cancel();
}

void
AudioOutputSource::Cancel() noexcept
{
	current_chunk = nullptr;
	pipe.Cancel();

	if (replay_gain_filter)
		replay_gain_filter->Reset();

	if (other_replay_gain_filter)
		other_replay_gain_filter->Reset();

	if (filter && !filter_flushed)
		filter->Reset();
}

inline void
AudioOutputSource::OpenFilter(AudioFormat audio_format,
			      PreparedFilter *prepared_replay_gain_filter,
			      PreparedFilter *prepared_other_replay_gain_filter,
			      PreparedFilter &prepared_filter)
try {
	assert(audio_format.IsValid());

	/* the replay_gain filter cannot fail here */
	if (prepared_other_replay_gain_filter) {
		other_replay_gain_serial = 0;
		other_replay_gain_filter =
			prepared_other_replay_gain_filter->Open(audio_format);
	}

	if (prepared_replay_gain_filter) {
		replay_gain_serial = 0;
		replay_gain_filter =
			prepared_replay_gain_filter->Open(audio_format);

		audio_format = replay_gain_filter->GetOutAudioFormat();

		assert(replay_gain_filter->GetOutAudioFormat() ==
		       other_replay_gain_filter->GetOutAudioFormat());
	}

	filter = prepared_filter.Open(audio_format);
	filter_flushed = false;
} catch (...) {
	CloseFilter();
	throw;
}

void
AudioOutputSource::CloseFilter() noexcept
{
	replay_gain_filter.reset();
	other_replay_gain_filter.reset();
	filter.reset();
}

std::span<const std::byte>
AudioOutputSource::GetChunkData(const MusicChunk &chunk,
				Filter *current_replay_gain_filter,
				unsigned *replay_gain_serial_p)
{
	assert(!chunk.IsEmpty());
	assert(chunk.CheckFormat(in_audio_format));

	auto data = chunk.ReadData();

	assert(data.size() % in_audio_format.GetFrameSize() == 0);

	if (!data.empty() && current_replay_gain_filter != nullptr) {
		replay_gain_filter_set_mode(*current_replay_gain_filter,
					    replay_gain_mode);

		if (chunk.replay_gain_serial != *replay_gain_serial_p) {
			replay_gain_filter_set_info(*current_replay_gain_filter,
						    chunk.replay_gain_serial != 0
						    ? &chunk.replay_gain_info
						    : nullptr);
			*replay_gain_serial_p = chunk.replay_gain_serial;
		}

		/* note: the ReplayGainFilter doesn't have a
		   ReadMore() method */
		data = current_replay_gain_filter->FilterPCM(data);
	}

	return data;
}

inline std::span<const std::byte>
AudioOutputSource::FilterChunk(const MusicChunk &chunk)
{
	assert(filter);
	assert(!filter_flushed);

	auto data = GetChunkData(chunk, replay_gain_filter.get(),
				 &replay_gain_serial);
	if (data.empty())
		return data;

	/* cross-fade */

	if (chunk.other != nullptr) {
		auto other_data = GetChunkData(*chunk.other,
					       other_replay_gain_filter.get(),
					       &other_replay_gain_serial);
		if (other_data.empty())
			return data;

		/* if the "other" chunk is longer, then that trailer
		   is used as-is, without mixing; it is part of the
		   "next" song being faded in, and if there's a rest,
		   it means cross-fading ends here */

		if (data.size() > other_data.size())
			data = data.first(other_data.size());

		float mix_ratio = chunk.mix_ratio;
		if (mix_ratio >= 0)
			/* reverse the mix ratio (because the
			   arguments to pcm_mix() are reversed), but
			   only if the mix ratio is non-negative; a
			   negative mix ratio is a MixRamp special
			   case */
			mix_ratio = 1.0f - mix_ratio;

		void *dest = cross_fade_buffer.Get(other_data.size());
		memcpy(dest, other_data.data(), other_data.size());
		if (!pcm_mix(cross_fade_dither, dest, data.data(), data.size(),
			     in_audio_format.format,
			     mix_ratio))
			throw FmtRuntimeError("Cannot cross-fade format {}",
					      in_audio_format.format);

		data = {(const std::byte *)dest, other_data.size()};
	}

	/* apply filter chain */

	return filter->FilterPCM(data);
}

bool
AudioOutputSource::Fill(Mutex &mutex)
{
	assert(filter);
	assert(!filter_flushed);

	if (current_chunk != nullptr && pending_tag == nullptr &&
	    pending_data.empty())
		DropCurrentChunk();

	if (current_chunk != nullptr)
		return true;

	current_chunk = pipe.Get();
	if (current_chunk == nullptr)
		return false;

	pending_tag = current_chunk->tag.get();

	try {
		/* release the mutex while the filter runs, because
		   that may take a while */
		const ScopeUnlock unlock(mutex);

		pending_data = FilterChunk(*current_chunk);
	} catch (...) {
		current_chunk = nullptr;
		throw;
	}

	return true;
}

void
AudioOutputSource::ConsumeData(size_t nbytes) noexcept
{
	assert(filter);
	assert(!filter_flushed);

	pending_data = pending_data.subspan(nbytes);

	if (pending_data.empty()) {
		/* give the filter a chance to return more data in
		   another buffer */
		pending_data = filter->ReadMore();

		if (pending_data.empty())
			DropCurrentChunk();
	}
}

std::span<const std::byte>
AudioOutputSource::Flush()
{
	assert(filter);

	filter_flushed = true;
	return filter->Flush();
}
