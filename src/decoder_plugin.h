/*
 * Copyright (C) 2003-2009 The Music Player Daemon Project
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef MPD_DECODER_PLUGIN_H
#define MPD_DECODER_PLUGIN_H

#include <stdbool.h>

struct input_stream;
struct tag;

/**
 * Opaque handle which the decoder plugin passes to the functions in
 * this header.
 */
struct decoder;

struct decoder_plugin {
	const char *name;

	/**
	 * optional, set this to NULL if the InputPlugin doesn't
	 * have/need one this must return < 0 if there is an error and
	 * >= 0 otherwise
	 */
	bool (*init)(void);

	/**
	 * optional, set this to NULL if the InputPlugin doesn't have/need one
	 */
	void (*finish)(void);

	/**
	 * this will be used to decode InputStreams, and is
	 * recommended for files and networked (HTTP) connections.
	 *
	 * @return false if the plugin cannot decode the stream, and
	 * true if it was able to do so (even if an error occured
	 * during playback)
	 */
	void (*stream_decode)(struct decoder *, struct input_stream *);

	/**
	 * use this if and only if your InputPlugin can only be passed
	 * a filename or handle as input, and will not allow callbacks
	 * to be set (like Ogg-Vorbis and FLAC libraries allow)
	 *
	 * @return false if the plugin cannot decode the file, and
	 * true if it was able to do so (even if an error occured
	 * during playback)
	 */
	void (*file_decode)(struct decoder *, const char *path);

	/**
	 * file should be the full path!  Returns NULL if a tag cannot
	 * be found or read
	 */
	struct tag *(*tag_dup)(const char *file);

	/* last element in these arrays must always be a NULL: */
	const char *const*suffixes;
	const char *const*mime_types;
};

#endif
