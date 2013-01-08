/*
 * Copyright (C) 2003-2012 The Music Player Daemon Project
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
#include "OggUtil.hxx"
#include "decoder_api.h"

bool
OggFeed(ogg_sync_state &oy, struct decoder *decoder,
	input_stream *input_stream, size_t size)
{
		char *buffer = ogg_sync_buffer(&oy, size);
		if (buffer == nullptr)
			return false;

		size_t nbytes = decoder_read(decoder, input_stream,
					     buffer, size);
		if (nbytes == 0)
			return false;

		ogg_sync_wrote(&oy, nbytes);
		return true;
}

bool
OggExpectPage(ogg_sync_state &oy, ogg_page &page,
	      decoder *decoder, input_stream *input_stream)
{
	while (true) {
		int r = ogg_sync_pageout(&oy, &page);
		if (r != 0)
			return r > 0;

		if (!OggFeed(oy, decoder, input_stream, 1024))
			return false;
	}
}

bool
OggExpectFirstPage(ogg_sync_state &oy, ogg_stream_state &os,
		   decoder *decoder, input_stream *is)
{
	ogg_page page;
	if (!OggExpectPage(oy, page, decoder, is))
		return false;

	ogg_stream_init(&os, ogg_page_serialno(&page));
	ogg_stream_pagein(&os, &page);
	return true;
}

bool
OggExpectPageIn(ogg_sync_state &oy, ogg_stream_state &os,
		decoder *decoder, input_stream *is)
{
	ogg_page page;
	if (!OggExpectPage(oy, page, decoder, is))
		return false;

	ogg_stream_pagein(&os, &page);
	return true;
}
