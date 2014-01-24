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

#include "config.h"
#include "filter/FilterPlugin.hxx"
#include "filter/FilterInternal.hxx"
#include "filter/FilterRegistry.hxx"
#include "pcm/PcmBuffer.hxx"
#include "AudioFormat.hxx"
#include "AudioCompress/compress.h"

#include <string.h>

class NormalizeFilter final : public Filter {
	struct Compressor *compressor;

	PcmBuffer buffer;

public:
	virtual AudioFormat Open(AudioFormat &af, Error &error) override;
	virtual void Close();
	virtual const void *FilterPCM(const void *src, size_t src_size,
				      size_t *dest_size_r, Error &error);
};

static Filter *
normalize_filter_init(gcc_unused const config_param &param,
		      gcc_unused Error &error)
{
	return new NormalizeFilter();
}

AudioFormat
NormalizeFilter::Open(AudioFormat &audio_format, gcc_unused Error &error)
{
	audio_format.format = SampleFormat::S16;

	compressor = Compressor_new(0);

	return audio_format;
}

void
NormalizeFilter::Close()
{
	buffer.Clear();
	Compressor_delete(compressor);
}

const void *
NormalizeFilter::FilterPCM(const void *src, size_t src_size,
			   size_t *dest_size_r, gcc_unused Error &error)
{
	int16_t *dest = (int16_t *)buffer.Get(src_size);
	memcpy(dest, src, src_size);

	Compressor_Process_int16(compressor, dest, src_size / 2);

	*dest_size_r = src_size;
	return dest;
}

const struct filter_plugin normalize_filter_plugin = {
	"normalize",
	normalize_filter_init,
};
