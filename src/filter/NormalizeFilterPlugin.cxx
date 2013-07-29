/*
 * Copyright (C) 2003-2013 The Music Player Daemon Project
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
#include "FilterPlugin.hxx"
#include "FilterInternal.hxx"
#include "FilterRegistry.hxx"
#include "pcm/PcmBuffer.hxx"
#include "audio_format.h"
#include "AudioCompress/compress.h"

#include <assert.h>
#include <string.h>

class NormalizeFilter final : public Filter {
	struct Compressor *compressor;

	PcmBuffer buffer;

public:
	virtual const audio_format *Open(audio_format &af, GError **error_r);
	virtual void Close();
	virtual const void *FilterPCM(const void *src, size_t src_size,
				      size_t *dest_size_r, GError **error_r);
};

static Filter *
normalize_filter_init(gcc_unused const struct config_param *param,
		      gcc_unused GError **error_r)
{
	return new NormalizeFilter();
}

const struct audio_format *
NormalizeFilter::Open(audio_format &audio_format, gcc_unused GError **error_r)
{
	audio_format.format = SAMPLE_FORMAT_S16;

	compressor = Compressor_new(0);

	return &audio_format;
}

void
NormalizeFilter::Close()
{
	buffer.Clear();
	Compressor_delete(compressor);
}

const void *
NormalizeFilter::FilterPCM(const void *src, size_t src_size,
			   size_t *dest_size_r, gcc_unused GError **error_r)
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
