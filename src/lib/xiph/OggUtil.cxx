/*
 * Copyright 2003-2016 The Music Player Daemon Project
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
#include "fs/io/Reader.hxx"

bool
OggFeed(ogg_sync_state &oy, Reader &reader, size_t size)
{
		char *buffer = ogg_sync_buffer(&oy, size);
		if (buffer == nullptr)
			return false;

		size_t nbytes = reader.Read(buffer, size);
		if (nbytes == 0)
			return false;

		ogg_sync_wrote(&oy, nbytes);
		return true;
}

bool
OggExpectPage(ogg_sync_state &oy, ogg_page &page, Reader &reader)
{
	while (true) {
		int r = ogg_sync_pageout(&oy, &page);
		if (r != 0)
			return r > 0;

		if (!OggFeed(oy, reader, 1024))
			return false;
	}
}

bool
OggExpectPageIn(ogg_sync_state &oy, ogg_stream_state &os, Reader &reader)
{
	ogg_page page;
	if (!OggExpectPage(oy, page, reader))
		return false;

	ogg_stream_pagein(&os, &page);
	return true;
}

bool
OggExpectPageSeek(ogg_sync_state &oy, ogg_page &page, Reader &reader)
{
	size_t remaining_skipped = 32768;

	while (true) {
		int r = ogg_sync_pageseek(&oy, &page);
		if (r > 0)
			return true;

		if (r < 0) {
			/* skipped -r bytes */
			size_t nbytes = -r;
			if (nbytes > remaining_skipped)
				/* still no ogg page - we lost our
				   patience, abort */
				return false;

			remaining_skipped -= nbytes;
			continue;
		}

		if (!OggFeed(oy, reader, 1024))
			return false;
	}
}

bool
OggExpectPageSeekIn(ogg_sync_state &oy, ogg_stream_state &os, Reader &reader)
{
	ogg_page page;
	if (!OggExpectPageSeek(oy, page, reader))
		return false;

	ogg_stream_pagein(&os, &page);
	return true;
}
