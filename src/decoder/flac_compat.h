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

#ifndef MPD_FLAC_COMPAT_H
#define MPD_FLAC_COMPAT_H

#include <FLAC/export.h>
#if !defined(FLAC_API_VERSION_CURRENT) || FLAC_API_VERSION_CURRENT <= 7
#  include <FLAC/seekable_stream_decoder.h>

/* starting with libFLAC 1.1.3, the SeekableStreamDecoder has been
   merged into the StreamDecoder.  The following macros try to emulate
   the new API for libFLAC 1.1.2 by mapping MPD's StreamDecoder calls
   to the old SeekableStreamDecoder API. */

#define FLAC__StreamDecoder FLAC__SeekableStreamDecoder
#define FLAC__stream_decoder_new FLAC__seekable_stream_decoder_new
#define FLAC__stream_decoder_get_decode_position FLAC__seekable_stream_decoder_get_decode_position
#define FLAC__stream_decoder_get_state FLAC__seekable_stream_decoder_get_state
#define FLAC__stream_decoder_process_single FLAC__seekable_stream_decoder_process_single
#define FLAC__stream_decoder_process_until_end_of_metadata FLAC__seekable_stream_decoder_process_until_end_of_metadata
#define FLAC__stream_decoder_seek_absolute FLAC__seekable_stream_decoder_seek_absolute
#define FLAC__stream_decoder_finish FLAC__seekable_stream_decoder_finish
#define FLAC__stream_decoder_delete FLAC__seekable_stream_decoder_delete

#define FLAC__STREAM_DECODER_END_OF_STREAM FLAC__SEEKABLE_STREAM_DECODER_END_OF_STREAM

typedef unsigned flac_read_status_size_t;

#define FLAC__StreamDecoderReadStatus FLAC__SeekableStreamDecoderReadStatus
#define FLAC__STREAM_DECODER_READ_STATUS_CONTINUE FLAC__SEEKABLE_STREAM_DECODER_READ_STATUS_OK
#define FLAC__STREAM_DECODER_READ_STATUS_END_OF_STREAM FLAC__SEEKABLE_STREAM_DECODER_READ_STATUS_OK
#define FLAC__STREAM_DECODER_READ_STATUS_ABORT FLAC__SEEKABLE_STREAM_DECODER_READ_STATUS_ERROR

#define FLAC__StreamDecoderSeekStatus FLAC__SeekableStreamDecoderSeekStatus
#define FLAC__STREAM_DECODER_SEEK_STATUS_OK FLAC__SEEKABLE_STREAM_DECODER_SEEK_STATUS_OK
#define FLAC__STREAM_DECODER_SEEK_STATUS_ERROR FLAC__SEEKABLE_STREAM_DECODER_SEEK_STATUS_ERROR
#define FLAC__STREAM_DECODER_SEEK_STATUS_UNSUPPORTED FLAC__SEEKABLE_STREAM_DECODER_SEEK_STATUS_ERROR

#define FLAC__StreamDecoderTellStatus FLAC__SeekableStreamDecoderTellStatus
#define FLAC__STREAM_DECODER_TELL_STATUS_OK FLAC__SEEKABLE_STREAM_DECODER_TELL_STATUS_OK
#define FLAC__STREAM_DECODER_TELL_STATUS_ERROR FLAC__SEEKABLE_STREAM_DECODER_TELL_STATUS_ERROR
#define FLAC__STREAM_DECODER_TELL_STATUS_UNSUPPORTED FLAC__SEEKABLE_STREAM_DECODER_TELL_STATUS_ERROR

#define FLAC__StreamDecoderLengthStatus FLAC__SeekableStreamDecoderLengthStatus
#define FLAC__STREAM_DECODER_LENGTH_STATUS_OK FLAC__SEEKABLE_STREAM_DECODER_LENGTH_STATUS_OK
#define FLAC__STREAM_DECODER_LENGTH_STATUS_ERROR FLAC__SEEKABLE_STREAM_DECODER_LENGTH_STATUS_ERROR
#define FLAC__STREAM_DECODER_LENGTH_STATUS_UNSUPPORTED FLAC__SEEKABLE_STREAM_DECODER_LENGTH_STATUS_ERROR

typedef enum {
	FLAC__STREAM_DECODER_INIT_STATUS_OK,
	FLAC__STREAM_DECODER_INIT_STATUS_ERROR,
} FLAC__StreamDecoderInitStatus;

static inline FLAC__StreamDecoderInitStatus
FLAC__stream_decoder_init_stream(FLAC__SeekableStreamDecoder *decoder,
				 FLAC__SeekableStreamDecoderReadCallback read_cb,
				 FLAC__SeekableStreamDecoderSeekCallback seek_cb,
				 FLAC__SeekableStreamDecoderTellCallback tell_cb,
				 FLAC__SeekableStreamDecoderLengthCallback length_cb,
				 FLAC__SeekableStreamDecoderEofCallback eof_cb,
				 FLAC__SeekableStreamDecoderWriteCallback write_cb,
				 FLAC__SeekableStreamDecoderMetadataCallback metadata_cb,
				 FLAC__SeekableStreamDecoderErrorCallback error_cb,
				 void *data)
{
	return FLAC__seekable_stream_decoder_set_read_callback(decoder, read_cb) &&
		FLAC__seekable_stream_decoder_set_seek_callback(decoder, seek_cb) &&
		FLAC__seekable_stream_decoder_set_tell_callback(decoder, tell_cb) &&
		FLAC__seekable_stream_decoder_set_length_callback(decoder, length_cb) &&
		FLAC__seekable_stream_decoder_set_eof_callback(decoder, eof_cb) &&
		FLAC__seekable_stream_decoder_set_write_callback(decoder, write_cb) &&
		FLAC__seekable_stream_decoder_set_metadata_callback(decoder, metadata_cb) &&
		FLAC__seekable_stream_decoder_set_metadata_respond(decoder, FLAC__METADATA_TYPE_VORBIS_COMMENT) &&
		FLAC__seekable_stream_decoder_set_error_callback(decoder, error_cb) &&
		FLAC__seekable_stream_decoder_set_client_data(decoder, data) &&
		FLAC__seekable_stream_decoder_init(decoder) == FLAC__SEEKABLE_STREAM_DECODER_OK
		? FLAC__STREAM_DECODER_INIT_STATUS_OK
		: FLAC__STREAM_DECODER_INIT_STATUS_ERROR;
}

#else /* FLAC_API_VERSION_CURRENT > 7 */

#  include <FLAC/stream_decoder.h>

#  define flac_init(a,b,c,d,e,f,g,h,i,j) \
        (FLAC__stream_decoder_init_stream(a,b,c,d,e,f,g,h,i,j) \
         == FLAC__STREAM_DECODER_INIT_STATUS_OK)

typedef size_t flac_read_status_size_t;

#endif /* FLAC_API_VERSION_CURRENT >= 7 */

#endif /* _FLAC_COMMON_H */
