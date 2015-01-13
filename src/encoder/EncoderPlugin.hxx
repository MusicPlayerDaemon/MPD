/*
 * Copyright (C) 2003-2015 The Music Player Daemon Project
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

#ifndef MPD_ENCODER_PLUGIN_HXX
#define MPD_ENCODER_PLUGIN_HXX

#include <stddef.h>

struct Encoder;
struct AudioFormat;
struct config_param;
struct Tag;
class Error;

struct EncoderPlugin {
	const char *name;

	Encoder *(*init)(const config_param &param,
			 Error &error);

	void (*finish)(Encoder *encoder);

	bool (*open)(Encoder *encoder,
		     AudioFormat &audio_format,
		     Error &error);

	void (*close)(Encoder *encoder);

	bool (*end)(Encoder *encoder, Error &error);

	bool (*flush)(Encoder *encoder, Error &error);

	bool (*pre_tag)(Encoder *encoder, Error &error);

	bool (*tag)(Encoder *encoder, const Tag &tag,
		    Error &error);

	bool (*write)(Encoder *encoder,
		      const void *data, size_t length,
		      Error &error);

	size_t (*read)(Encoder *encoder, void *dest, size_t length);

	const char *(*get_mime_type)(Encoder *encoder);
};

/**
 * Creates a new encoder object.
 *
 * @param plugin the encoder plugin
 * @param param optional configuration
 * @param error location to store the error occurring, or nullptr to ignore errors.
 * @return an encoder object on success, nullptr on failure
 */
static inline Encoder *
encoder_init(const EncoderPlugin &plugin, const config_param &param,
	     Error &error_r)
{
	return plugin.init(param, error_r);
}

#endif
