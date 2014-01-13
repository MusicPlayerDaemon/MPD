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

#ifndef MPD_ENCODER_PLUGIN_HXX
#define MPD_ENCODER_PLUGIN_HXX

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>

struct EncoderPlugin;
struct AudioFormat;
struct config_param;
struct Tag;
class Error;

struct Encoder {
	const EncoderPlugin &plugin;

#ifndef NDEBUG
	bool open, pre_tag, tag, end;
#endif

	explicit Encoder(const EncoderPlugin &_plugin)
		:plugin(_plugin)
#ifndef NDEBUG
		, open(false)
#endif
	{}
};

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

	bool (*tag)(Encoder *encoder, const Tag *tag,
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

/**
 * Frees an encoder object.
 *
 * @param encoder the encoder
 */
static inline void
encoder_finish(Encoder *encoder)
{
	assert(!encoder->open);

	encoder->plugin.finish(encoder);
}

/**
 * Opens an encoder object.  You must call this prior to using it.
 * Before you free it, you must call encoder_close().  You may open
 * and close (reuse) one encoder any number of times.
 *
 * After this function returns successfully and before the first
 * encoder_write() call, you should invoke encoder_read() to obtain
 * the file header.
 *
 * @param encoder the encoder
 * @param audio_format the encoder's input audio format; the plugin
 * may modify the struct to adapt it to its abilities
 * @return true on success
 */
static inline bool
encoder_open(Encoder *encoder, AudioFormat &audio_format,
	     Error &error)
{
	assert(!encoder->open);

	bool success = encoder->plugin.open(encoder, audio_format, error);
#ifndef NDEBUG
	encoder->open = success;
	encoder->pre_tag = encoder->tag = encoder->end = false;
#endif
	return success;
}

/**
 * Closes an encoder object.  This disables the encoder, and readies
 * it for reusal by calling encoder_open() again.
 *
 * @param encoder the encoder
 */
static inline void
encoder_close(Encoder *encoder)
{
	assert(encoder->open);

	if (encoder->plugin.close != nullptr)
		encoder->plugin.close(encoder);

#ifndef NDEBUG
	encoder->open = false;
#endif
}

/**
 * Ends the stream: flushes the encoder object, generate an
 * end-of-stream marker (if applicable), make everything which might
 * currently be buffered available by encoder_read().
 *
 * After this function has been called, the encoder may not be usable
 * for more data, and only encoder_read() and encoder_close() can be
 * called.
 *
 * @param encoder the encoder
 * @return true on success
 */
static inline bool
encoder_end(Encoder *encoder, Error &error)
{
	assert(encoder->open);
	assert(!encoder->end);

#ifndef NDEBUG
	encoder->end = true;
#endif

	/* this method is optional */
	return encoder->plugin.end != nullptr
		? encoder->plugin.end(encoder, error)
		: true;
}

/**
 * Flushes an encoder object, make everything which might currently be
 * buffered available by encoder_read().
 *
 * @param encoder the encoder
 * @return true on success
 */
static inline bool
encoder_flush(Encoder *encoder, Error &error)
{
	assert(encoder->open);
	assert(!encoder->pre_tag);
	assert(!encoder->tag);
	assert(!encoder->end);

	/* this method is optional */
	return encoder->plugin.flush != nullptr
		? encoder->plugin.flush(encoder, error)
		: true;
}

/**
 * Prepare for sending a tag to the encoder.  This is used by some
 * encoders to flush the previous sub-stream, in preparation to begin
 * a new one.
 *
 * @param encoder the encoder
 * @param tag the tag object
 * @return true on success
 */
static inline bool
encoder_pre_tag(Encoder *encoder, Error &error)
{
	assert(encoder->open);
	assert(!encoder->pre_tag);
	assert(!encoder->tag);
	assert(!encoder->end);

	/* this method is optional */
	bool success = encoder->plugin.pre_tag != nullptr
		? encoder->plugin.pre_tag(encoder, error)
		: true;

#ifndef NDEBUG
	encoder->pre_tag = success;
#endif
	return success;
}

/**
 * Sends a tag to the encoder.
 *
 * Instructions: call encoder_pre_tag(); then obtain flushed data with
 * encoder_read(); finally call encoder_tag().
 *
 * @param encoder the encoder
 * @param tag the tag object
 * @return true on success
 */
static inline bool
encoder_tag(Encoder *encoder, const Tag *tag, Error &error)
{
	assert(encoder->open);
	assert(!encoder->pre_tag);
	assert(encoder->tag);
	assert(!encoder->end);

#ifndef NDEBUG
	encoder->tag = false;
#endif

	/* this method is optional */
	return encoder->plugin.tag != nullptr
		? encoder->plugin.tag(encoder, tag, error)
		: true;
}

/**
 * Writes raw PCM data to the encoder.
 *
 * @param encoder the encoder
 * @param data the buffer containing PCM samples
 * @param length the length of the buffer in bytes
 * @return true on success
 */
static inline bool
encoder_write(Encoder *encoder, const void *data, size_t length,
	      Error &error)
{
	assert(encoder->open);
	assert(!encoder->pre_tag);
	assert(!encoder->tag);
	assert(!encoder->end);

	return encoder->plugin.write(encoder, data, length, error);
}

/**
 * Reads encoded data from the encoder.
 *
 * Call this repeatedly until no more data is returned.
 *
 * @param encoder the encoder
 * @param dest the destination buffer to copy to
 * @param length the maximum length of the destination buffer
 * @return the number of bytes written to #dest
 */
static inline size_t
encoder_read(Encoder *encoder, void *dest, size_t length)
{
	assert(encoder->open);
	assert(!encoder->pre_tag || !encoder->tag);

#ifndef NDEBUG
	if (encoder->pre_tag) {
		encoder->pre_tag = false;
		encoder->tag = true;
	}
#endif

	return encoder->plugin.read(encoder, dest, length);
}

/**
 * Get mime type of encoded content.
 *
 * @param plugin the encoder plugin
 * @return an constant string, nullptr on failure
 */
static inline const char *
encoder_get_mime_type(Encoder *encoder)
{
	/* this method is optional */
	return encoder->plugin.get_mime_type != nullptr
		? encoder->plugin.get_mime_type(encoder)
		: nullptr;
}

#endif
