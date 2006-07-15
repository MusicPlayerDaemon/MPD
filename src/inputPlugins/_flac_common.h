/* the Music Player Daemon (MPD)
 * (c)2003-2006 by Warren Dukes (warren.dukes@gmail.com)
 * This project's homepage is: http://www.musicpd.org
 *
 * Common data structures and functions used by FLAC and OggFLAC
 * (c) 2005 by Eric Wong <normalperson@yhbt.net>
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

#ifndef _FLAC_COMMON_H
#define _FLAC_COMMON_H

#include "../inputPlugin.h"

#if defined(HAVE_FLAC) || defined(HAVE_OGGFLAC)

#include "../tag.h"
#include "../inputStream.h"
#include "../outputBuffer.h"
#include "../decode.h"
#include <FLAC/seekable_stream_decoder.h>
#include <FLAC/metadata.h>

#define FLAC_CHUNK_SIZE 4080

typedef struct {
	unsigned char chunk[FLAC_CHUNK_SIZE];
	int chunk_length;
	float time;
	int bitRate;
	FLAC__uint64 position;
	OutputBuffer * cb;
	DecoderControl * dc;
	InputStream * inStream;
	ReplayGainInfo * replayGainInfo;
	MpdTag * tag;
} FlacData;

/* initializes a given FlacData struct */
void init_FlacData (FlacData * data, OutputBuffer * cb,
		DecoderControl * dc, InputStream * inStream);
void flac_metadata_common_cb(	const FLAC__StreamMetadata *block,
				FlacData *data);
void flac_error_common_cb(	const char * plugin,
				FLAC__StreamDecoderErrorStatus status,
				FlacData *data);

MpdTag * copyVorbisCommentBlockToMpdTag(const FLAC__StreamMetadata * block, 
		MpdTag * tag);

/* keep this inlined, this is just macro but prettier :) */
static inline int flacSendChunk(FlacData * data)
{
	if (sendDataToOutputBuffer(data->cb, NULL, data->dc, 1, data->chunk,
			data->chunk_length, data->time, data->bitRate,
			data->replayGainInfo) == OUTPUT_BUFFER_DC_STOP)
		return -1;

	return 0;
}


#endif /* HAVE_FLAC || HAVE_OGGFLAC */

#endif /* _FLAC_COMMON_H */

