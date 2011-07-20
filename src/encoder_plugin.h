/*
 * Copyright (C) 2003-2010 The Music Player Daemon Project
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

#ifndef MPD_ENCODER_PLUGIN_H
#define MPD_ENCODER_PLUGIN_H

#include <glib.h>

#include <stdbool.h>
#include <stddef.h>

struct encoder_plugin;
struct audio_format;
struct config_param;
struct tag;

struct encoder {
	const struct encoder_plugin *plugin;
};

struct encoder_plugin {
	const char *name;

	struct encoder *(*init)(const struct config_param *param,
				GError **error);

	void (*finish)(struct encoder *encoder);

	bool (*open)(struct encoder *encoder,
		     struct audio_format *audio_format,
		     GError **error);

	void (*close)(struct encoder *encoder);

	bool (*flush)(struct encoder *encoder, GError **error);

	bool (*pre_tag)(struct encoder *encoder, GError **error);

	bool (*tag)(struct encoder *encoder, const struct tag *tag,
		    GError **error);

	bool (*write)(struct encoder *encoder,
		      const void *data, size_t length,
		      GError **error);

	size_t (*read)(struct encoder *encoder, void *dest, size_t length);

	const char *(*get_mime_type)(struct encoder *encoder);
};

/**
 * Initializes an encoder object.  This should be used by encoder
 * plugins to initialize their base class.
 */
static inline void
encoder_struct_init(struct encoder *encoder,
		    const struct encoder_plugin *plugin)
{
	encoder->plugin = plugin;
}

/**
 * Creates a new encoder object.
 *
 * @param plugin the encoder plugin
 * @param param optional configuration
 * @param error location to store the error occuring, or NULL to ignore errors.
 * @return an encoder object on success, NULL on failure
 */
static inline struct encoder *
encoder_init(const struct encoder_plugin *plugin,
	     const struct config_param *param, GError **error)
{
	return plugin->init(param, error);
}

/**
 * Frees an encoder object.
 *
 * @param encoder the encoder
 */
static inline void
encoder_finish(struct encoder *encoder)
{
	encoder->plugin->finish(encoder);
}

/**
 * Opens an encoder object.  You must call this prior to using it.
 * Before you free it, you must call encoder_close().  You may open
 * and close (reuse) one encoder any number of times.
 *
 * @param encoder the encoder
 * @param audio_format the encoder's input audio format; the plugin
 * may modify the struct to adapt it to its abilities
 * @param error location to store the error occuring, or NULL to ignore errors.
 * @return true on success
 */
static inline bool
encoder_open(struct encoder *encoder, struct audio_format *audio_format,
	     GError **error)
{
	return encoder->plugin->open(encoder, audio_format, error);
}

/**
 * Closes an encoder object.  This disables the encoder, and readies
 * it for reusal by calling encoder_open() again.
 *
 * @param encoder the encoder
 */
static inline void
encoder_close(struct encoder *encoder)
{
	if (encoder->plugin->close != NULL)
		encoder->plugin->close(encoder);
}

/**
 * Flushes an encoder object, make everything which might currently be
 * buffered available by encoder_read().
 *
 * @param encoder the encoder
 * @param error location to store the error occuring, or NULL to ignore errors.
 * @return true on success
 */
static inline bool
encoder_flush(struct encoder *encoder, GError **error)
{
	/* this method is optional */
	return encoder->plugin->flush != NULL
		? encoder->plugin->flush(encoder, error)
		: true;
}

/**
 * Prepare for sending a tag to the encoder.  This is used by some
 * encoders to flush the previous sub-stream, in preparation to begin
 * a new one.
 *
 * @param encoder the encoder
 * @param tag the tag object
 * @param error location to store the error occuring, or NULL to ignore errors.
 * @return true on success
 */
static inline bool
encoder_pre_tag(struct encoder *encoder, GError **error)
{
	/* this method is optional */
	return encoder->plugin->pre_tag != NULL
		? encoder->plugin->pre_tag(encoder, error)
		: true;
}

/**
 * Sends a tag to the encoder.
 *
 * Instructions: call encoder_pre_tag(); then obtain flushed data with
 * encoder_read(); finally call encoder_tag().
 *
 * @param encoder the encoder
 * @param tag the tag object
 * @param error location to store the error occuring, or NULL to ignore errors.
 * @return true on success
 */
static inline bool
encoder_tag(struct encoder *encoder, const struct tag *tag, GError **error)
{
	/* this method is optional */
	return encoder->plugin->tag != NULL
		? encoder->plugin->tag(encoder, tag, error)
		: true;
}

/**
 * Writes raw PCM data to the encoder.
 *
 * @param encoder the encoder
 * @param data the buffer containing PCM samples
 * @param length the length of the buffer in bytes
 * @param error location to store the error occuring, or NULL to ignore errors.
 * @return true on success
 */
static inline bool
encoder_write(struct encoder *encoder, const void *data, size_t length,
	      GError **error)
{
	return encoder->plugin->write(encoder, data, length, error);
}

/**
 * Reads encoded data from the encoder.
 *
 * @param encoder the encoder
 * @param dest the destination buffer to copy to
 * @param length the maximum length of the destination buffer
 * @return the number of bytes written to #dest
 */
static inline size_t
encoder_read(struct encoder *encoder, void *dest, size_t length)
{
	return encoder->plugin->read(encoder, dest, length);
}

/**
 * Get mime type of encoded content.
 *
 * @param plugin the encoder plugin
 * @return an constant string, NULL on failure
 */
static inline const char *
encoder_get_mime_type(struct encoder *encoder)
{
	/* this method is optional */
	return encoder->plugin->get_mime_type != NULL
		? encoder->plugin->get_mime_type(encoder)
		: NULL;
}

#endif
