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

#ifndef MPD_ENCODER_INTERFACE_HXX
#define MPD_ENCODER_INTERFACE_HXX

#include "EncoderPlugin.hxx"
#include "Compiler.h"

#include <assert.h>
#include <stddef.h>

struct Tag;

class Encoder {
	const bool implements_tag;

public:
	explicit Encoder(bool _implements_tag)
		:implements_tag(_implements_tag) {}
	virtual ~Encoder() {}

	bool ImplementsTag() const {
		return implements_tag;
	}

	/**
	 * Ends the stream: flushes the encoder object, generate an
	 * end-of-stream marker (if applicable), make everything which
	 * might currently be buffered available by encoder_read().
	 *
	 * After this function has been called, the encoder may not be
	 * usable for more data, and only Read() and Close() can be
	 * called.
	 *
	 * @return true on success
	 */
	virtual bool End(gcc_unused Error &error) {
		return true;
	}

	/**
	 * Flushes an encoder object, make everything which might
	 * currently be buffered available by Read().
	 *
	 * @return true on success
	 */
	virtual bool Flush(gcc_unused Error &error) {
		return true;
	}

	/**
	 * Prepare for sending a tag to the encoder.  This is used by
	 * some encoders to flush the previous sub-stream, in
	 * preparation to begin a new one.
	 *
	 * @return true on success
	 */
	virtual bool PreTag(gcc_unused Error &error) {
		return true;
	}

	/**
	 * Sends a tag to the encoder.
	 *
	 * Instructions: call PreTag(); then obtain flushed data with
	 * Read(); finally call Tag().
	 *
	 * @param tag the tag object
	 * @return true on success
	 */
	virtual bool SendTag(gcc_unused const Tag &tag,
			     gcc_unused Error &error) {
		return true;
	}

	/**
	 * Writes raw PCM data to the encoder.
	 *
	 * @param data the buffer containing PCM samples
	 * @param length the length of the buffer in bytes
	 * @return true on success
	 */
	virtual bool Write(const void *data, size_t length,
			   Error &error) = 0;

	/**
	 * Reads encoded data from the encoder.
	 *
	 * Call this repeatedly until no more data is returned.
	 *
	 * @param dest the destination buffer to copy to
	 * @param length the maximum length of the destination buffer
	 * @return the number of bytes written to #dest
	 */
	virtual size_t Read(void *dest, size_t length) = 0;
};

struct PreparedEncoder {
	const EncoderPlugin &plugin;

	explicit PreparedEncoder(const EncoderPlugin &_plugin)
		:plugin(_plugin) {}


	/**
	 * Frees an #Encoder object.
	 */
	void Dispose() {
		plugin.finish(this);
	}

	/**
	 * Opens the object.  You must call this prior to using it.
	 * Before you free it, you must call Close().  You may open
	 * and close (reuse) one encoder any number of times.
	 *
	 * After this function returns successfully and before the
	 * first encoder_write() call, you should invoke
	 * encoder_read() to obtain the file header.
	 *
	 * @param audio_format the encoder's input audio format; the plugin
	 * may modify the struct to adapt it to its abilities
	 * @return true on success
	 */
	Encoder *Open(AudioFormat &audio_format, Error &error) {
		return plugin.open(this, audio_format, error);
	}

};

/**
 * Get mime type of encoded content.
 *
 * @return an constant string, nullptr on failure
 */
static inline const char *
encoder_get_mime_type(PreparedEncoder *encoder)
{
	/* this method is optional */
	return encoder->plugin.get_mime_type != nullptr
		? encoder->plugin.get_mime_type(encoder)
		: nullptr;
}

#endif
