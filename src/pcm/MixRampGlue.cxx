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

#include "MixRampGlue.hxx"
#include "MixRampAnalyzer.hxx"
#include "AudioFormat.hxx"
#include "MusicPipe.hxx"
#include "MusicChunk.hxx"
#include "util/Compiler.h"

#include <stdio.h>

static std::string
StartToString(const MixRampArray &a) noexcept
{
	std::string s;

	MixRampItem last{};
	for (const auto &i : a) {
		if (i.time < FloatDuration{} || i == last)
			continue;

		char buffer[64];
		sprintf(buffer, "%.2f %.2f;", i.volume, i.time.count());
		last = i;

		s.append(buffer);
	}

	return s;
}

static std::string
EndToString(const MixRampArray &a, FloatDuration total_time) noexcept
{
	std::string s;

	MixRampItem last{};
	for (const auto &i : a) {
		if (i.time < FloatDuration{} || i == last)
			continue;

		char buffer[64];
		sprintf(buffer, "%.2f %.2f;",
			i.volume, (total_time - i.time).count());
		last = i;

		s.append(buffer);
	}

	return s;
}

static std::string
ToString(const MixRampData &mr, FloatDuration total_time,
	 MixRampDirection direction) noexcept
{
	switch (direction) {
	case MixRampDirection::START:
		return StartToString(mr.start);

	case MixRampDirection::END:
		return EndToString(mr.end, total_time);
	}

	gcc_unreachable();
}

std::string
AnalyzeMixRamp(const MusicPipe &pipe, const AudioFormat &audio_format,
	       MixRampDirection direction) noexcept
{
	if (audio_format.sample_rate != ReplayGainAnalyzer::SAMPLE_RATE ||
	    audio_format.channels != ReplayGainAnalyzer::CHANNELS ||
	    audio_format.format != SampleFormat::FLOAT)
		// TODO: auto-convert
		return {};

	const auto *chunk = pipe.Peek();
	if (chunk == nullptr)
		return {};

	MixRampAnalyzer a;
	do {
		a.Process(ConstBuffer<ReplayGainAnalyzer::Frame>::FromVoid({chunk->data, chunk->length}));
	} while ((chunk = chunk->next.get()) != nullptr);

	return ToString(a.GetResult(), a.GetTime(), direction);
}
