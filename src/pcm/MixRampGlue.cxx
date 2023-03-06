// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "MixRampGlue.hxx"
#include "MixRampAnalyzer.hxx"
#include "AudioFormat.hxx"
#include "MusicPipe.hxx"
#include "MusicChunk.hxx"
#include "util/Compiler.h"
#include "util/SpanCast.hxx"

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
		a.Process(FromBytesStrict<const ReplayGainAnalyzer::Frame>({chunk->data, chunk->length}));
	} while ((chunk = chunk->next.get()) != nullptr);

	return ToString(a.GetResult(), a.GetTime(), direction);
}
