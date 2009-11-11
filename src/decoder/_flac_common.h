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
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

/*
 * Common data structures and functions used by FLAC and OggFLAC
 */

#ifndef MPD_FLAC_COMMON_H
#define MPD_FLAC_COMMON_H

#include "decoder_api.h"
#include "pcm_buffer.h"
#include "config.h"

#include <glib.h>

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "flac"

#include <FLAC/export.h>
#if !defined(FLAC_API_VERSION_CURRENT) || FLAC_API_VERSION_CURRENT <= 7
#  include <FLAC/seekable_stream_decoder.h>
#  define flac_decoder           FLAC__SeekableStreamDecoder
#  define flac_new()             FLAC__seekable_stream_decoder_new()

#  define flac_ogg_init(a,b,c,d,e,f,g,h,i,j) (0)

#  define flac_get_decode_position(x,y) \
                 FLAC__seekable_stream_decoder_get_decode_position(x,y)
#  define flac_get_state(x)      FLAC__seekable_stream_decoder_get_state(x)
#  define flac_process_single(x) FLAC__seekable_stream_decoder_process_single(x)
#  define flac_process_metadata(x) \
                 FLAC__seekable_stream_decoder_process_until_end_of_metadata(x)
#  define flac_seek_absolute(x,y) \
                 FLAC__seekable_stream_decoder_seek_absolute(x,y)
#  define flac_finish(x)         FLAC__seekable_stream_decoder_finish(x)
#  define flac_delete(x)         FLAC__seekable_stream_decoder_delete(x)

#  define flac_decoder_eof       FLAC__SEEKABLE_STREAM_DECODER_END_OF_STREAM

typedef unsigned flac_read_status_size_t;
#  define flac_read_status       FLAC__SeekableStreamDecoderReadStatus
#  define flac_read_status_continue \
                                 FLAC__SEEKABLE_STREAM_DECODER_READ_STATUS_OK
#  define flac_read_status_eof   FLAC__SEEKABLE_STREAM_DECODER_READ_STATUS_OK
#  define flac_read_status_abort \
                               FLAC__SEEKABLE_STREAM_DECODER_READ_STATUS_ERROR

#  define flac_seek_status       FLAC__SeekableStreamDecoderSeekStatus
#  define flac_seek_status_ok    FLAC__SEEKABLE_STREAM_DECODER_SEEK_STATUS_OK
#  define flac_seek_status_error FLAC__SEEKABLE_STREAM_DECODER_SEEK_STATUS_ERROR

#  define flac_tell_status         FLAC__SeekableStreamDecoderTellStatus
#  define flac_tell_status_ok      FLAC__SEEKABLE_STREAM_DECODER_TELL_STATUS_OK
#  define flac_tell_status_error \
                                FLAC__SEEKABLE_STREAM_DECODER_TELL_STATUS_ERROR
#  define flac_tell_status_unsupported \
                                FLAC__SEEKABLE_STREAM_DECODER_TELL_STATUS_ERROR

#  define flac_length_status       FLAC__SeekableStreamDecoderLengthStatus
#  define flac_length_status_ok  FLAC__SEEKABLE_STREAM_DECODER_LENGTH_STATUS_OK
#  define flac_length_status_error \
                              FLAC__SEEKABLE_STREAM_DECODER_LENGTH_STATUS_ERROR
#  define flac_length_status_unsupported \
                              FLAC__SEEKABLE_STREAM_DECODER_LENGTH_STATUS_ERROR

#  ifdef HAVE_OGGFLAC
#    include <OggFLAC/seekable_stream_decoder.h>
#  endif
#else /* FLAC_API_VERSION_CURRENT > 7 */

/*
 * OggFLAC support is handled by our flac_plugin already, and
 * thus we *can* always have it if libFLAC was compiled with it
 */
#  include "_ogg_common.h"

#  include <FLAC/stream_decoder.h>
#  define flac_decoder           FLAC__StreamDecoder
#  define flac_new()             FLAC__stream_decoder_new()

#  define flac_init(a,b,c,d,e,f,g,h,i,j) \
        (FLAC__stream_decoder_init_stream(a,b,c,d,e,f,g,h,i,j) \
         == FLAC__STREAM_DECODER_INIT_STATUS_OK)
#  define flac_ogg_init(a,b,c,d,e,f,g,h,i,j) \
        (FLAC__stream_decoder_init_ogg_stream(a,b,c,d,e,f,g,h,i,j) \
         == FLAC__STREAM_DECODER_INIT_STATUS_OK)

#  define flac_get_decode_position(x,y) \
                 FLAC__stream_decoder_get_decode_position(x,y)
#  define flac_get_state(x)      FLAC__stream_decoder_get_state(x)
#  define flac_process_single(x) FLAC__stream_decoder_process_single(x)
#  define flac_process_metadata(x) \
                          FLAC__stream_decoder_process_until_end_of_metadata(x)
#  define flac_seek_absolute(x,y)  FLAC__stream_decoder_seek_absolute(x,y)
#  define flac_finish(x)         FLAC__stream_decoder_finish(x)
#  define flac_delete(x)         FLAC__stream_decoder_delete(x)

#  define flac_decoder_eof       FLAC__STREAM_DECODER_END_OF_STREAM

typedef size_t flac_read_status_size_t;
#  define flac_read_status       FLAC__StreamDecoderReadStatus
#  define flac_read_status_continue \
                                 FLAC__STREAM_DECODER_READ_STATUS_CONTINUE
#  define flac_read_status_eof   FLAC__STREAM_DECODER_READ_STATUS_END_OF_STREAM
#  define flac_read_status_abort FLAC__STREAM_DECODER_READ_STATUS_ABORT

#  define flac_seek_status       FLAC__StreamDecoderSeekStatus
#  define flac_seek_status_ok    FLAC__STREAM_DECODER_SEEK_STATUS_OK
#  define flac_seek_status_error FLAC__STREAM_DECODER_SEEK_STATUS_ERROR
#  define flac_seek_status_unsupported \
                                 FLAC__STREAM_DECODER_SEEK_STATUS_UNSUPPORTED

#  define flac_tell_status         FLAC__StreamDecoderTellStatus
#  define flac_tell_status_ok      FLAC__STREAM_DECODER_TELL_STATUS_OK
#  define flac_tell_status_error   FLAC__STREAM_DECODER_TELL_STATUS_ERROR
#  define flac_tell_status_unsupported \
                                   FLAC__STREAM_DECODER_TELL_STATUS_UNSUPPORTED

#  define flac_length_status       FLAC__StreamDecoderLengthStatus
#  define flac_length_status_ok    FLAC__STREAM_DECODER_LENGTH_STATUS_OK
#  define flac_length_status_error FLAC__STREAM_DECODER_LENGTH_STATUS_ERROR
#  define flac_length_status_unsupported \
                                 FLAC__STREAM_DECODER_LENGTH_STATUS_UNSUPPORTED

#endif /* FLAC_API_VERSION_CURRENT >= 7 */

#include <FLAC/metadata.h>

#define FLAC_CHUNK_SIZE 4080

struct flac_data {
	struct pcm_buffer buffer;

	/**
	 * The number of the next frame which is going to be decoded.
	 */
	FLAC__uint64 next_frame;

	float time;
	unsigned int bit_rate;
	struct audio_format audio_format;
	float total_time;
	FLAC__uint64 position;
	struct decoder *decoder;
	struct input_stream *input_stream;
	struct replay_gain_info *replay_gain_info;
	struct tag *tag;
};

/* initializes a given FlacData struct */
void
flac_data_init(struct flac_data *data, struct decoder * decoder,
	       struct input_stream *input_stream);

void
flac_data_deinit(struct flac_data *data);

void flac_metadata_common_cb(const FLAC__StreamMetadata * block,
			     struct flac_data *data);

void flac_error_common_cb(const char *plugin,
			  FLAC__StreamDecoderErrorStatus status,
			  struct flac_data *data);

FLAC__StreamDecoderWriteStatus
flac_common_write(struct flac_data *data, const FLAC__Frame * frame,
		  const FLAC__int32 *const buf[]);

#if defined(FLAC_API_VERSION_CURRENT) && FLAC_API_VERSION_CURRENT > 7

char*
flac_cue_track(		const char* pathname,
			const unsigned int tnum);

unsigned int
flac_vtrack_tnum(	const char*);

#endif /* FLAC_API_VERSION_CURRENT >= 7 */

#endif /* _FLAC_COMMON_H */
