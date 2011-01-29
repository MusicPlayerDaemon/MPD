/*
 * Copyright (C) 2003-2011 The Music Player Daemon Project
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

#ifndef CONVERT_FILTER_PLUGIN_H
#define CONVERT_FILTER_PLUGIN_H

struct filter;
struct audio_format;

/**
 * Sets the output audio format for the specified filter.  You must
 * call this after the filter has been opened.  Since this audio
 * format switch is a violation of the filter API, this filter must be
 * the last in a chain.
 */
void
convert_filter_set(struct filter *filter,
		   const struct audio_format *out_audio_format);

#endif
