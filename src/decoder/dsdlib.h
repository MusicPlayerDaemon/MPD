/*
 * Copyright (C) 2012 The Music Player Daemon Project
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

#ifndef MPD_DECODER_DSDLIB_H
#define MPD_DECODER_DSDLIB_H

struct dsdlib_id {
	char value[4];
};

bool
dsdlib_id_equals(const struct dsdlib_id *id, const char *s);

bool
dsdlib_read(struct decoder *decoder, struct input_stream *is,
	    void *data, size_t length);

bool
dsdlib_skip_to(struct decoder *decoder, struct input_stream *is,
	       goffset offset);

bool
dsdlib_skip(struct decoder *decoder, struct input_stream *is,
	    goffset delta);

#endif
