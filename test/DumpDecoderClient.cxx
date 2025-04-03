// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "DumpDecoderClient.hxx"
#include "lib/fmt/AudioFormatFormatter.hxx"
#include "decoder/DecoderAPI.hxx"
#include "input/InputStream.hxx"
#include "tag/Names.hxx"
#include "util/StringBuffer.hxx"

#include <unistd.h>
#include <stdio.h>

void
DumpDecoderClient::Ready(const AudioFormat audio_format,
			 bool seekable,
			 SignedSongTime duration) noexcept
{
	assert(!initialized);
	assert(audio_format.IsValid());

	fmt::print(stderr, "audio_format={} duration={} seekable={}\n",
		   audio_format, duration.ToDoubleS(), seekable);

	initialized = true;
}

DecoderCommand
DumpDecoderClient::GetCommand() noexcept
{
	return DecoderCommand::NONE;
}

void
DumpDecoderClient::CommandFinished() noexcept
{
}

SongTime
DumpDecoderClient::GetSeekTime() noexcept
{
	return SongTime();
}

uint64_t
DumpDecoderClient::GetSeekFrame() noexcept
{
	return 1;
}

void
DumpDecoderClient::SeekError() noexcept
{
}

InputStreamPtr
DumpDecoderClient::OpenUri(std::string_view uri)
{
	return InputStream::OpenReady(uri, mutex);
}

size_t
DumpDecoderClient::Read(InputStream &is, std::span<std::byte> dest) noexcept
{
	try {
		return is.LockRead(dest);
	} catch (...) {
		return 0;
	}
}

void
DumpDecoderClient::SubmitTimestamp([[maybe_unused]] FloatDuration t) noexcept
{
}

DecoderCommand
DumpDecoderClient::SubmitAudio([[maybe_unused]] InputStream *is,
			       std::span<const std::byte> audio,
			       [[maybe_unused]] uint16_t kbit_rate) noexcept
{
	if (kbit_rate != prev_kbit_rate) {
		prev_kbit_rate = kbit_rate;
		fmt::print(stderr, "{} kbit/s\n", kbit_rate);
	}

	[[maybe_unused]] ssize_t nbytes = write(STDOUT_FILENO,
						audio.data(), audio.size());
	return GetCommand();
}

DecoderCommand
DumpDecoderClient::SubmitTag([[maybe_unused]] InputStream *is,
			     Tag &&tag) noexcept
{
	fmt::print(stderr, "TAG: duration={}\n", tag.duration.ToDoubleS());

	for (const auto &i : tag)
		fmt::print(stderr, "  {}={:?}\n", tag_item_names[i.type], i.value);

	return GetCommand();
}

static void
DumpReplayGainTuple(const char *name, const ReplayGainTuple &tuple) noexcept
{
	if (tuple.IsDefined())
		fmt::print(stderr, "replay_gain[{}]: gain={} peak={}\n",
			   name, tuple.gain, tuple.peak);
}

static void
DumpReplayGainInfo(const ReplayGainInfo &info) noexcept
{
	DumpReplayGainTuple("album", info.album);
	DumpReplayGainTuple("track", info.track);
}

void
DumpDecoderClient::SubmitReplayGain(const ReplayGainInfo *rgi) noexcept
{
	if (rgi != nullptr)
		DumpReplayGainInfo(*rgi);
}

void
DumpDecoderClient::SubmitMixRamp([[maybe_unused]] MixRampInfo &&mix_ramp) noexcept
{
	fmt::print(stderr, "MixRamp: start={:?} end={:?}\n",
		   mix_ramp.GetStart(), mix_ramp.GetEnd());
}
