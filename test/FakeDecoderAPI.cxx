/*
 * Copyright 2003-2017 The Music Player Daemon Project
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
#include "FakeDecoderAPI.hxx"
#include "decoder/DecoderAPI.hxx"
#include "input/InputStream.hxx"
#include "util/StringBuffer.hxx"
#include "Compiler.h"

#include <stdexcept>

#include <unistd.h>
#include <stdio.h>

void
FakeDecoder::Ready(const AudioFormat audio_format,
		   gcc_unused bool seekable,
		   SignedSongTime duration)
{
	assert(!initialized);
	assert(audio_format.IsValid());

	fprintf(stderr, "audio_format=%s duration=%f\n",
		ToString(audio_format).c_str(),
		duration.ToDoubleS());

	initialized = true;
}

DecoderCommand
FakeDecoder::GetCommand()
{
	return DecoderCommand::NONE;
}

void
FakeDecoder::CommandFinished()
{
}

SongTime
FakeDecoder::GetSeekTime()
{
	return SongTime();
}

uint64_t
FakeDecoder::GetSeekFrame()
{
	return 1;
}

void
FakeDecoder::SeekError()
{
}

InputStreamPtr
FakeDecoder::OpenUri(const char *uri)
{
	return InputStream::OpenReady(uri, mutex, cond);
}

size_t
FakeDecoder::Read(InputStream &is, void *buffer, size_t length)
{
	try {
		return is.LockRead(buffer, length);
	} catch (const std::runtime_error &e) {
		return 0;
	}
}

void
FakeDecoder::SubmitTimestamp(gcc_unused double t)
{
}

DecoderCommand
FakeDecoder::SubmitData(gcc_unused InputStream *is,
			const void *data, size_t datalen,
			gcc_unused uint16_t kbit_rate)
{
	static uint16_t prev_kbit_rate;
	if (kbit_rate != prev_kbit_rate) {
		prev_kbit_rate = kbit_rate;
		fprintf(stderr, "%u kbit/s\n", kbit_rate);
	}

	gcc_unused ssize_t nbytes = write(1, data, datalen);
	return DecoderCommand::NONE;
}

DecoderCommand
FakeDecoder::SubmitTag(gcc_unused InputStream *is,
		       Tag &&tag)
{
	fprintf(stderr, "TAG: duration=%f\n", tag.duration.ToDoubleS());

	for (const auto &i : tag)
		fprintf(stderr, "  %s=%s\n", tag_item_names[i.type], i.value);

	return DecoderCommand::NONE;
}

static void
DumpReplayGainTuple(const char *name, const ReplayGainTuple &tuple)
{
	if (tuple.IsDefined())
		fprintf(stderr, "replay_gain[%s]: gain=%f peak=%f\n",
			name, tuple.gain, tuple.peak);
}

static void
DumpReplayGainInfo(const ReplayGainInfo &info)
{
	DumpReplayGainTuple("album", info.album);
	DumpReplayGainTuple("track", info.track);
}

void
FakeDecoder::SubmitReplayGain(const ReplayGainInfo *rgi)
{
	if (rgi != nullptr)
		DumpReplayGainInfo(*rgi);
}

void
FakeDecoder::SubmitMixRamp(gcc_unused MixRampInfo &&mix_ramp)
{
	fprintf(stderr, "MixRamp: start='%s' end='%s'\n",
		mix_ramp.GetStart(), mix_ramp.GetEnd());
}
