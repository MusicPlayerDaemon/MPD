// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "AudioFormat.hxx"
#include "util/StringBuffer.hxx"

#include <cassert>

#include <stdio.h>

void
AudioFormat::ApplyMask(AudioFormat mask) noexcept
{
	assert(IsValid());
	assert(mask.IsMaskValid());

	if (mask.sample_rate != 0)
		sample_rate = mask.sample_rate;

	if (mask.format != SampleFormat::UNDEFINED)
		format = mask.format;

	if (mask.channels != 0)
		channels = mask.channels;

	assert(IsValid());
}

StringBuffer<24>
ToString(const AudioFormat af) noexcept
{
	StringBuffer<24> buffer;
	char *p = buffer.data();

	if (af.format == SampleFormat::DSD && af.sample_rate > 0 &&
	    af.sample_rate % 44100 == 0) {
		/* use shortcuts such as "dsd64" which implies the
		   sample rate */
		p += sprintf(p, "dsd%u:", af.sample_rate * 8 / 44100);
	} else {
		const char *sample_format = af.format != SampleFormat::UNDEFINED
			? sample_format_to_string(af.format)
			: "*";

		if (af.sample_rate > 0)
			p += sprintf(p, "%u:%s:", af.sample_rate,
				     sample_format);
		else
			p += sprintf(p, "*:%s:", sample_format);
	}

	if (af.channels > 0)
		p += sprintf(p, "%u", af.channels);
	else {
		*p++ = '*';
		*p = 0;
	}

	return buffer;
}
