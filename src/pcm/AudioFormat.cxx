/*
 * Copyright 2003-2020 The Music Player Daemon Project
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

#include "AudioFormat.hxx"
#include "util/StringBuffer.hxx"

#include <assert.h>
#include <stdio.h>
#include <map>

/**
 * If source is based on 44.1k get nearest valid 44.1k variant from the target sample rate.
 * For exmple if the source is 44.1k and the target is 96k it will return 88.2k
 */
unsigned
determine_selective_resample_rate(unsigned source_rate, unsigned target_rate) noexcept;

unsigned
determine_selective_resample_rate(unsigned source_rate, unsigned target_rate) noexcept
{
	unsigned out_sample_rate = source_rate;
	const std::map<unsigned, unsigned> lut48to41 = {
		{384000, 352800 },
		{192000, 176400 },
	 	 {96000,  88200 },
		 {48000,  44100 }
	};	

	if( source_rate % 44100 == 0 && lut48to41.find(target_rate) != lut48to41.end() )
		out_sample_rate = lut48to41.find(target_rate)->second;
	else if(target_rate)
		out_sample_rate = target_rate;

    return out_sample_rate;
}

void
AudioFormat::ApplyMask(AudioFormat mask, bool selective_44k_resample) noexcept
{
	assert(IsValid());
	assert(mask.IsMaskValid());

	if (mask.sample_rate != 0) {
		if(selective_44k_resample)
			sample_rate = determine_selective_resample_rate(sample_rate, mask.sample_rate);
		else
			sample_rate = mask.sample_rate;
	}

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
