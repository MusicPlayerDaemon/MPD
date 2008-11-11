/* the Music Player Daemon (MPD)
 * Copyright (C) 2003-2007 by Warren Dukes (warren.dukes@gmail.com)
 * Copyright (C) 2008 Max Kellermann <max@duempel.org>
 * This project's homepage is: http://www.musicpd.org
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

#ifndef MPD_DECODER_API_H
#define MPD_DECODER_API_H

/*
 * This is the public API which is used by decoder plugins to
 * communicate with the mpd core.
 *
 */

#include "input_stream.h"
#include "replay_gain.h"
#include "tag.h"
#include "tag_id3.h"
#include "audio_format.h"
#include "playerData.h"

#include <stdbool.h>

enum decoder_command {
	DECODE_COMMAND_NONE = 0,
	DECODE_COMMAND_START,
	DECODE_COMMAND_STOP,
	DECODE_COMMAND_SEEK
};


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


/**
 * Opaque handle which the decoder plugin passes to the functions in
 * this header.
 */
struct decoder;


/**
 * Notify the player thread that it has finished initialization and
 * that it has read the song's meta data.
 */
void decoder_initialized(struct decoder * decoder,
			 const struct audio_format *audio_format,
			 bool seekable, float total_time);

const char *decoder_get_url(struct decoder * decoder, char * buffer);

enum decoder_command decoder_get_command(struct decoder * decoder);

/**
 * Called by the decoder when it has performed the requested command
 * (dc->command).  This function resets dc->command and wakes up the
 * player thread.
 */
void decoder_command_finished(struct decoder * decoder);

double decoder_seek_where(struct decoder * decoder);

void decoder_seek_error(struct decoder * decoder);

/**
 * Blocking read from the input stream.  Returns the number of bytes
 * read, or 0 if one of the following occurs: end of file; error;
 * command (like SEEK or STOP).
 */
size_t decoder_read(struct decoder *decoder,
		    struct input_stream *inStream,
		    void *buffer, size_t length);

/**
 * This function is called by the decoder plugin when it has
 * successfully decoded block of input data.
 *
 * We send inStream for buffering the inputStream while waiting to
 * send the next chunk
 */
enum decoder_command
decoder_data(struct decoder *decoder,
	     struct input_stream *inStream,
	     void *data, size_t datalen, float data_time, uint16_t bitRate,
	     struct replay_gain_info *replay_gain_info);

/**
 * This function is called by the decoder plugin when it has
 * successfully decoded a tag.
 *
 * @param is an input stream which is buffering while we are waiting
 * for the player
 */
enum decoder_command
decoder_tag(struct decoder *decoder, struct input_stream *is,
	    const struct tag *tag);

#endif
