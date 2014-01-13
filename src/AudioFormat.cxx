/*
 * Copyright (C) 2003-2014 The Music Player Daemon Project
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

#include <assert.h>
#include <stdio.h>

void
AudioFormat::ApplyMask(AudioFormat mask)
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

const char *
sample_format_to_string(SampleFormat format)
{
	switch (format) {
	case SampleFormat::UNDEFINED:
		return "?";

	case SampleFormat::S8:
		return "8";

	case SampleFormat::S16:
		return "16";

	case SampleFormat::S24_P32:
		return "24";

	case SampleFormat::S32:
		return "32";

	case SampleFormat::FLOAT:
		return "f";

	case SampleFormat::DSD:
		return "dsd";
	}

	/* unreachable */
	assert(false);
	gcc_unreachable();
}

const char *
audio_format_to_string(const AudioFormat af,
		       struct audio_format_string *s)
{
	assert(s != nullptr);

	snprintf(s->buffer, sizeof(s->buffer), "%u:%s:%u",
		 af.sample_rate, sample_format_to_string(af.format),
		 af.channels);

	return s->buffer;
}
