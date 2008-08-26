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

#ifndef DECODER_API_H
#define DECODER_API_H

/*
 * This is the public API which is used by decoder plugins to
 * communicate with the mpd core.
 *
 */

#include "inputStream.h"
#include "replayGain.h"
#include "tag.h"
#include "playerData.h"


/* valid values for streamTypes in the InputPlugin struct: */
#define INPUT_PLUGIN_STREAM_FILE	0x01
#define INPUT_PLUGIN_STREAM_URL		0x02


struct decoder;


/* optional, set this to NULL if the InputPlugin doesn't have/need one
 * this must return < 0 if there is an error and >= 0 otherwise */
typedef int (*decoder_init_func) (void);

/* optional, set this to NULL if the InputPlugin doesn't have/need one */
typedef void (*decoder_finish_func) (void);

/* boolean return value, returns 1 if the InputStream is decodable by
 * the InputPlugin, 0 if not */
typedef unsigned int (*decoder_try_decode_func) (InputStream *);

/* this will be used to decode InputStreams, and is recommended for files
 * and networked (HTTP) connections.
 *
 * returns -1 on error, 0 on success */
typedef int (*decoder_stream_decode_func) (struct decoder *, InputStream *);

/* use this if and only if your InputPlugin can only be passed a filename or
 * handle as input, and will not allow callbacks to be set (like Ogg-Vorbis
 * and FLAC libraries allow)
 *
 * returns -1 on error, 0 on success */
typedef int (*decoder_file_decode_func) (struct decoder *, char *path);

/* file should be the full path!  Returns NULL if a tag cannot be found
 * or read */
typedef MpdTag *(*decoder_tag_dup_func) (char *file);

struct decoder_plugin {
	const char *name;

	decoder_init_func initFunc;
	decoder_finish_func finishFunc;
	decoder_try_decode_func tryDecodeFunc;
	decoder_stream_decode_func streamDecodeFunc;
	decoder_file_decode_func fileDecodeFunc;
	decoder_tag_dup_func tagDupFunc;

	/* one or more of the INPUT_PLUGIN_STREAM_* values OR'd together */
	unsigned char streamTypes;

	/* last element in these arrays must always be a NULL: */
	const char *const*suffixes;
	const char *const*mimeTypes;
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
			 const AudioFormat * audio_format,
			 float total_time);

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
 * This function is called by the decoder plugin when it has
 * successfully decoded block of input data.
 *
 * We send inStream for buffering the inputStream while waiting to
 * send the next chunk
 */
enum decoder_command
decoder_data(struct decoder *decoder, InputStream * inStream, int seekable,
	     void *data, size_t datalen, float data_time, mpd_uint16 bitRate,
	     ReplayGainInfo * replayGainInfo);

void decoder_flush(struct decoder *decoder);

void decoder_clear(struct decoder *decoder);

#endif
