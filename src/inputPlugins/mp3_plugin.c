/* the Music Player Daemon (MPD)
 * (c)2003-2004 by Warren Dukes (shank@mercury.chem.pitt.edu)
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

#include "../inputPlugin.h"

#ifdef HAVE_MAD

#include "../pcm_utils.h"
#ifdef USE_MPD_MAD
#include "../libmad/mad.h"
#else
#include <mad.h>
#endif
#ifdef HAVE_ID3TAG
#ifdef USE_MPD_ID3TAG
#include "../libid3tag/id3tag.h"
#else
#include <id3tag.h>
#endif
#endif
#include "../log.h"
#include "../utils.h"
#include "../tag.h"

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>

#define FRAMES_CUSHION		2000

#define READ_BUFFER_SIZE	40960

#define DECODE_SKIP		-3
#define DECODE_BREAK		-2
#define DECODE_CONT		-1
#define DECODE_OK		0

#define MUTEFRAME_SKIP          1
#define MUTEFRAME_SEEK          2

/* this is stolen from mpg321! */
struct audio_dither {
	mad_fixed_t error[3];
	mad_fixed_t random;
};

unsigned long prng(unsigned long state) {
	return (state * 0x0019660dL + 0x3c6ef35fL) & 0xffffffffL;
}

signed long audio_linear_dither(unsigned int bits, mad_fixed_t sample, struct audio_dither *dither) {
	unsigned int scalebits;
	mad_fixed_t output, mask, random;

	enum {
		MIN = -MAD_F_ONE,
		MAX =  MAD_F_ONE - 1
	};

	sample += dither->error[0] - dither->error[1] + dither->error[2];

	dither->error[2] = dither->error[1];
	dither->error[1] = dither->error[0] / 2;

	output = sample + (1L << (MAD_F_FRACBITS + 1 - bits - 1));

	scalebits = MAD_F_FRACBITS + 1 - bits;
	mask = (1L << scalebits) - 1;

	random  = prng(dither->random);
	output += (random & mask) - (dither->random & mask);

	dither->random = random;

	if (output > MAX) {
		output = MAX;

		if (sample > MAX)
			sample = MAX;
	}
	else if (output < MIN) {
	        output = MIN;

		if (sample < MIN)
			sample = MIN;
	}

	output &= ~mask;

	dither->error[0] = sample - output;

	return output >> scalebits;
}
/* end of stolen stuff from mpg321 */

/* decoder stuff is based on madlld */

#define MP3_DATA_OUTPUT_BUFFER_SIZE 4096

typedef struct _mp3DecodeData {
	struct mad_stream stream;
	struct mad_frame frame;
	struct mad_synth synth;
	mad_timer_t timer;
	unsigned char readBuffer[READ_BUFFER_SIZE];
	char outputBuffer[MP3_DATA_OUTPUT_BUFFER_SIZE];
	char * outputPtr;
	char * outputBufferEnd;
	float totalTime;
	float elapsedTime;
	int muteFrame;
	long * frameOffset;
	mad_timer_t * times;
	long highestFrame;
	long maxFrames;
	long currentFrame;
	int flush;
	unsigned long bitRate;
	InputStream * inStream;
	struct audio_dither dither;
} mp3DecodeData;

void initMp3DecodeData(mp3DecodeData * data, InputStream * inStream) {
	data->outputPtr = data->outputBuffer;
	data->outputBufferEnd = data->outputBuffer+MP3_DATA_OUTPUT_BUFFER_SIZE;
	data->muteFrame = 0;
	data->highestFrame = 0;
	data->maxFrames = 0;
	data->frameOffset = NULL;
	data->times = NULL;
	data->currentFrame = 0;
	data->flush = 1;
        data->inStream = inStream;
	memset(&(data->dither), 0, sizeof(struct audio_dither));

	mad_stream_init(&data->stream);
        data->stream.options |= MAD_OPTION_IGNORECRC;
	mad_frame_init(&data->frame);
	mad_synth_init(&data->synth);
	mad_timer_reset(&data->timer);
}

int seekMp3InputBuffer(mp3DecodeData * data, long offset) {
	if(seekInputStream(data->inStream,offset,SEEK_SET) < 0) {
                return -1;
        }

	mad_stream_buffer(&data->stream,data->readBuffer,0);
	(data->stream).error = 0;

	return 0;
}

int fillMp3InputBuffer(mp3DecodeData * data) {
	size_t readSize;
	size_t remaining;
        size_t readed;
	unsigned char * readStart;

	if((data->stream).next_frame!=NULL) {
		remaining = (data->stream).bufend-(data->stream).next_frame;
		memmove(data->readBuffer,(data->stream).next_frame,remaining);
		readStart = (data->readBuffer)+remaining;
		readSize = READ_BUFFER_SIZE-remaining;
	}
	else {
		readSize = READ_BUFFER_SIZE;
		readStart = data->readBuffer,
		remaining = 0;
	}

	/* we've exhausted the read buffer, so give up!, these potential
	 * mp3 frames are way too big, and thus unlikely to be mp3 frames */
	if(readSize == 0) return -1;

	readed = readFromInputStream(data->inStream, readStart, (size_t)1, 
			readSize);
	if(readed <= 0 && inputStreamAtEOF(data->inStream)) return -1;
	/* sleep for a fraction of a second! */
	else if(readed <= 0) {
		readed = 0;
		my_usleep(10000);
	}

	mad_stream_buffer(&data->stream,data->readBuffer,readed+remaining);
	(data->stream).error = 0;

	return 0;
}

#ifdef HAVE_ID3TAG
static MpdTag * mp3_parseId3Tag(mp3DecodeData * data, signed long tagsize) {
	MpdTag * ret = NULL;
	struct id3_tag * id3Tag = NULL;
	id3_length_t count;
	id3_byte_t const *id3_data;
	id3_byte_t * allocated = NULL;

	count = data->stream.bufend - data->stream.this_frame;

	if(tagsize <= count) {
		id3_data = data->stream.this_frame;
		mad_stream_skip(&(data->stream), tagsize);
	}
	else {
		allocated = malloc(tagsize);
		if(!allocated) goto fail;

		memcpy(allocated, data->stream.this_frame, count);
		mad_stream_skip(&(data->stream), count);

		while(count < tagsize) {
			int len;

			len = readFromInputStream(data->inStream, 
				allocated+count, (size_t)1, 
				tagsize-count);
			if(len <= 0 && inputStreamAtEOF(data->inStream))				{
				break;
			}
			else if(len <= 0) my_usleep(10000);
			else count += len;
		}

		if(count != tagsize) {
			DEBUG("mp3_decode: error parsing ID3 tag\n");
			goto fail;
		}

		id3_data = allocated;
	}

	id3Tag = id3_tag_parse(id3_data, tagsize);
 
	if(id3Tag) {
		ret = parseId3Tag(id3Tag);
		id3_tag_delete(id3Tag);
	}

fail:
	if(allocated) free(allocated);
	return ret;
}
#endif

int decodeNextFrameHeader(mp3DecodeData * data, MpdTag ** tag) {
	if((data->stream).buffer==NULL || (data->stream).error==MAD_ERROR_BUFLEN) {
		if(fillMp3InputBuffer(data) < 0) {
			return DECODE_BREAK;
		}
	}
	if(mad_header_decode(&data->frame.header,&data->stream)) {
#ifdef HAVE_ID3TAG
		if((data->stream).error==MAD_ERROR_LOSTSYNC && 
				(data->stream).this_frame) 
		{
			signed long tagsize = id3_tag_query(
					(data->stream).this_frame,
					(data->stream).bufend-
					(data->stream).this_frame);

			if(tagsize>0) {
				if(tag && !(*tag)) {
					*tag = mp3_parseId3Tag(data, tagsize);
					
				}
				else {
					mad_stream_skip(&(data->stream),
							tagsize);
				}
				return DECODE_CONT;
			}
		}
#endif
		if(MAD_RECOVERABLE((data->stream).error)) {
			return DECODE_SKIP;
		}
		else {
			if((data->stream).error==MAD_ERROR_BUFLEN) return DECODE_CONT;
			else
			{
				ERROR("unrecoverable frame level error "
					"(%s).\n",
					mad_stream_errorstr(&data->stream));
				data->flush = 0;
				return DECODE_BREAK;
			}
		}
	}
	if(data->frame.header.layer != MAD_LAYER_III) {
		return DECODE_SKIP;
	}

	return DECODE_OK;
}

int decodeNextFrame(mp3DecodeData * data) {
	if((data->stream).buffer==NULL || (data->stream).error==MAD_ERROR_BUFLEN) {
		if(fillMp3InputBuffer(data) < 0) {
			return DECODE_BREAK;
		}
	}
	if(mad_frame_decode(&data->frame,&data->stream)) {
#ifdef HAVE_ID3TAG
		if((data->stream).error==MAD_ERROR_LOSTSYNC) {
			signed long tagsize = id3_tag_query(
					(data->stream).this_frame,
					(data->stream).bufend-
					(data->stream).this_frame);
			if(tagsize>0) {
				mad_stream_skip(&(data->stream),tagsize);
				return DECODE_CONT;
			}
		}
#endif
		if(MAD_RECOVERABLE((data->stream).error)) {
			return DECODE_SKIP;
		}
		else {
			if((data->stream).error==MAD_ERROR_BUFLEN) return DECODE_CONT;
			else
			{
				ERROR("unrecoverable frame level error "
					"(%s).\n",
					mad_stream_errorstr(&data->stream));
				data->flush = 0;
				return DECODE_BREAK;
			}
		}
	}

	return DECODE_OK;
}

/* xing stuff stolen from alsaplayer */
# define XING_MAGIC	(('X' << 24) | ('i' << 16) | ('n' << 8) | 'g')

struct xing {
  	long flags;			/* valid fields (see below) */
  	unsigned long frames;		/* total number of frames */
  	unsigned long bytes;		/* total number of bytes */
  	unsigned char toc[100];		/* 100-point seek table */
  	long scale;			/* ?? */
};

enum {
  	XING_FRAMES = 0x00000001L,
  	XING_BYTES  = 0x00000002L,
  	XING_TOC    = 0x00000004L,
  	XING_SCALE  = 0x00000008L
};

int parse_xing(struct xing *xing, struct mad_bitptr ptr, unsigned int bitlen)
{
  	if (bitlen < 64 || mad_bit_read(&ptr, 32) != XING_MAGIC) goto fail;

  	xing->flags = mad_bit_read(&ptr, 32);
  	bitlen -= 64;

  	if (xing->flags & XING_FRAMES) {
    		if (bitlen < 32) goto fail;
    		xing->frames = mad_bit_read(&ptr, 32);
    		bitlen -= 32;
  	}

  	if (xing->flags & XING_BYTES) {
    		if (bitlen < 32) goto fail;
    		xing->bytes = mad_bit_read(&ptr, 32);
    		bitlen -= 32;
  	}

  	if (xing->flags & XING_TOC) {
    		int i;
    		if (bitlen < 800) goto fail;
      		for (i = 0; i < 100; ++i) xing->toc[i] = mad_bit_read(&ptr, 8);
    		bitlen -= 800;
  	}

  	if (xing->flags & XING_SCALE) {
    		if (bitlen < 32) goto fail;
    		xing->scale = mad_bit_read(&ptr, 32);
    		bitlen -= 32;
  	}

 	 return 1;

fail:
  	xing->flags = 0;
  	return 0;
}

int decodeFirstFrame(mp3DecodeData * data, DecoderControl * dc,
		MpdTag ** tag) 
{
	struct xing xing;
	int ret;
	int skip;

	memset(&xing,0,sizeof(struct xing));
	xing.flags = 0;

	while(1) {
		skip = 0;
		while((ret = decodeNextFrameHeader(data, tag))==DECODE_CONT && 
				(!dc || !dc->stop));
		if(ret==DECODE_SKIP) skip = 1;
		else if(ret==DECODE_BREAK || (dc && dc->stop)) return -1;
		while((ret = decodeNextFrame(data))==DECODE_CONT && 
				(!dc || !dc->stop));
		if(ret==DECODE_BREAK || (dc && dc->stop)) return -1;
		if(!skip && ret==DECODE_OK) break;
	}

	if(parse_xing(&xing,data->stream.anc_ptr,data->stream.anc_bitlen)) {
		if(xing.flags & XING_FRAMES) {
			mad_timer_t duration = data->frame.header.duration;
			mad_timer_multiply(&duration,xing.frames);
			data->muteFrame = MUTEFRAME_SKIP;
			data->totalTime = ((float)mad_timer_count(duration,
						MAD_UNITS_MILLISECONDS))/1000;
			data->maxFrames = xing.frames;
		}
	}
	else {
		size_t offset = data->inStream->offset;
		mad_timer_t duration = data->frame.header.duration;
		float frameTime = ((float)mad_timer_count(duration,
					MAD_UNITS_MILLISECONDS))/1000;
		if(data->stream.this_frame!=NULL) {
			offset-= data->stream.bufend-data->stream.this_frame;
		}
		else {
			offset-= data->stream.bufend-data->stream.buffer;
		}
		if(data->inStream->size >= offset) {
			data->totalTime = ((data->inStream->size-offset)*8.0)/
					(data->frame).header.bitrate;
			data->maxFrames = 
				data->totalTime/frameTime+FRAMES_CUSHION;
		}
		else {
			data->maxFrames = FRAMES_CUSHION;
			data->totalTime = 0;
		}
	}

	data->frameOffset = malloc(sizeof(long)*data->maxFrames);
	data->times = malloc(sizeof(mad_timer_t)*data->maxFrames);

	return 0;
}

void mp3DecodeDataFinalize(mp3DecodeData * data) {
	mad_synth_finish(&data->synth);
	mad_frame_finish(&data->frame);
	mad_stream_finish(&data->stream);

	if(data->frameOffset) free(data->frameOffset);
	if(data->times) free(data->times);
}

/* this is primarily used for getting total time for tags */
int getMp3TotalTime(char * file) {
        InputStream inStream;
	mp3DecodeData data;
	int ret;

        if(openInputStream(&inStream, file) < 0) return -1;
	initMp3DecodeData(&data,&inStream);
	if(decodeFirstFrame(&data, NULL, NULL)<0) ret = -1;
	else ret = data.totalTime+0.5;
	mp3DecodeDataFinalize(&data);
	closeInputStream(&inStream);

	return ret;
}

int openMp3FromInputStream(InputStream * inStream, mp3DecodeData * data,
		DecoderControl * dc, MpdTag ** tag) 
{
	initMp3DecodeData(data, inStream);
	*tag = NULL;
	if(decodeFirstFrame(data, dc, tag)<0) {
		mp3DecodeDataFinalize(data);
		if(tag && *tag) freeMpdTag(*tag);
		return -1;
	}

	return 0;
}

int mp3Read(mp3DecodeData * data, OutputBuffer * cb, DecoderControl * dc) {
	int i;
	int ret;
	int skip;

	if(data->currentFrame>=data->highestFrame) { 
		mad_timer_add(&data->timer,(data->frame).header.duration);
		data->bitRate = (data->frame).header.bitrate;
		if(data->currentFrame>=data->maxFrames) {
			data->currentFrame = data->maxFrames - 1;
		}
		else data->highestFrame++;
		data->frameOffset[data->currentFrame] = data->inStream->offset;
		if(data->stream.this_frame!=NULL) {
			data->frameOffset[data->currentFrame]-= 
					data->stream.bufend-
					data->stream.this_frame;
		}
		else {
			data->frameOffset[data->currentFrame]-= 
					data->stream.bufend-data->stream.buffer;
		}
		data->times[data->currentFrame] = data->timer;
	}
	else data->timer = data->times[data->currentFrame];
	data->currentFrame++;
	data->elapsedTime = ((float)mad_timer_count(data->timer,MAD_UNITS_MILLISECONDS))/1000;

	switch(data->muteFrame) {
        case MUTEFRAME_SKIP:
		data->muteFrame = 0;
                break;
        case MUTEFRAME_SEEK:
		if(dc->seekWhere<=data->elapsedTime) {
                        data->outputPtr = data->outputBuffer;
                        clearOutputBuffer(cb);
			data->muteFrame = 0;
			dc->seek = 0;
		}
                break;
        default:
		mad_synth_frame(&data->synth,&data->frame);

		if(data->inStream->metaTitle) {
			MpdTag * tag = newMpdTag();
			if(data->inStream->metaName) {
				addItemToMpdTag(tag,
						TAG_ITEM_NAME,
						data->inStream->metaName);
			}
			addItemToMpdTag(tag, TAG_ITEM_TITLE,
					data->inStream->metaTitle);
			free(data->inStream->metaTitle);
			data->inStream->metaTitle = NULL;
			copyMpdTagToOutputBuffer(cb, tag);
			freeMpdTag(tag);
		}

		for(i=0;i<(data->synth).pcm.length;i++) {
			mpd_sint16 * sample;

			sample = (mpd_sint16 *)data->outputPtr;	
			*sample = (mpd_sint16) audio_linear_dither(16,
					(data->synth).pcm.samples[0][i],
					&(data->dither));
			data->outputPtr+=2;

			if(MAD_NCHANNELS(&(data->frame).header)==2) {
				sample = (mpd_sint16 *)data->outputPtr;	
				*sample = (mpd_sint16) audio_linear_dither(16,
						(data->synth).pcm.samples[1][i],
						&(data->dither));
				data->outputPtr+=2;
			}

			if(data->outputPtr>=data->outputBufferEnd) {
                                long ret;
                                ret = sendDataToOutputBuffer(cb,
                                                data->inStream,
                                                dc,
                                                data->inStream->seekable,
                                                data->outputBuffer,
                                                data->outputPtr-
						data->outputBuffer,
                                                data->elapsedTime,
                                                data->bitRate/1000,
						NULL);
                                if(ret == OUTPUT_BUFFER_DC_STOP) {
					data->flush = 0;
                                        return DECODE_BREAK;
                                }

                                data->outputPtr = data->outputBuffer;

                                if(ret == OUTPUT_BUFFER_DC_SEEK) break;
			}
		}

		if(dc->seek && data->inStream->seekable) {
			long i = 0;
			data->muteFrame = MUTEFRAME_SEEK;
			while(i<data->highestFrame && dc->seekWhere >
					((float)mad_timer_count(data->times[i],
					MAD_UNITS_MILLISECONDS))/1000) 
			{
				i++;
			}
			if(i<data->highestFrame) {
				if(seekMp3InputBuffer(data,
						data->frameOffset[i]) == 0)
                                {
                                        data->outputPtr = data->outputBuffer;
                                        clearOutputBuffer(cb);
				        data->currentFrame = i;
                                }
                                else dc->seekError = 1;
				data->muteFrame = 0;
				dc->seek = 0;
			}
		}
                else if(dc->seek && !data->inStream->seekable) {
                        dc->seek = 0;
                        dc->seekError = 1;
                }
	}

	while(1) {
		skip = 0;
		while((ret = decodeNextFrameHeader(data, NULL))==DECODE_CONT &&
				!dc->stop && !dc->seek);
		if(ret==DECODE_BREAK || dc->stop || dc->seek) break;
		else if(ret==DECODE_SKIP) skip = 1;
		if(!data->muteFrame) {
			while((ret = decodeNextFrame(data))==DECODE_CONT &&
					!dc->stop && !dc->seek);
			if(ret==DECODE_BREAK || dc->stop || dc->seek) break;
		}
		if(!skip && ret==DECODE_OK) break;
	}

	if(dc->stop) return DECODE_BREAK;

	return ret;
}

void initAudioFormatFromMp3DecodeData(mp3DecodeData * data, AudioFormat * af) {
	af->bits = 16;
	af->sampleRate = (data->frame).header.samplerate;
	af->channels = MAD_NCHANNELS(&(data->frame).header);
}

int mp3_decode(OutputBuffer * cb, DecoderControl * dc, InputStream * inStream) {
	mp3DecodeData data;
	MpdTag * tag = NULL;

	if(openMp3FromInputStream(inStream, &data, dc, &tag) < 0) {
		closeInputStream(inStream);
		if(!dc->stop) {
                        ERROR("Input does not appear to be a mp3 bit stream.\n");
		        return -1;
                }
                else {
                        dc->state = DECODE_STATE_STOP;
                        dc->stop = 0;
                }
                return 0;
	}

	initAudioFormatFromMp3DecodeData(&data, &(dc->audioFormat));
        getOutputAudioFormat(&(dc->audioFormat), &(cb->audioFormat));
        
	dc->totalTime = data.totalTime;

	if(inStream->metaTitle) {
		if(tag) freeMpdTag(tag);
		tag = newMpdTag();
		addItemToMpdTag(tag, TAG_ITEM_TITLE, inStream->metaTitle);
		free(inStream->metaTitle);
		inStream->metaTitle = NULL;
		if(inStream->metaName) {
			addItemToMpdTag(tag, TAG_ITEM_NAME, 
					inStream->metaName);
		}
		copyMpdTagToOutputBuffer(cb, tag);
		freeMpdTag(tag);
	}
	else if(tag) {
		if(inStream->metaName) {
			clearItemsFromMpdTag(tag, TAG_ITEM_NAME);
			addItemToMpdTag(tag, TAG_ITEM_NAME, 
					inStream->metaName);
		}
		copyMpdTagToOutputBuffer(cb, tag);
		freeMpdTag(tag);
	}
	else if(inStream->metaName) {
		tag = newMpdTag();
		if(inStream->metaName) {
			addItemToMpdTag(tag, TAG_ITEM_NAME, 
					inStream->metaName);
		}
		copyMpdTagToOutputBuffer(cb, tag);
		freeMpdTag(tag);
	}

	dc->state = DECODE_STATE_DECODE;

	while(mp3Read(&data,cb,dc)!=DECODE_BREAK);
	/* send last little bit if not dc->stop */
	if(!dc->stop && data.outputPtr!=data.outputBuffer && data.flush)  {
        	sendDataToOutputBuffer(cb, NULL, dc, 
                                data.inStream->seekable,
                                data.outputBuffer,
                                data.outputPtr-data.outputBuffer,
                                data.elapsedTime,data.bitRate/1000,
				NULL);
	}

	closeInputStream(inStream);

	if(dc->seek && data.muteFrame == MUTEFRAME_SEEK) {
                clearOutputBuffer(cb);
                dc->seek = 0;
        }

	flushOutputBuffer(cb);
	mp3DecodeDataFinalize(&data);

	if(dc->stop) {
		dc->state = DECODE_STATE_STOP;
		dc->stop = 0;
	}
	else dc->state = DECODE_STATE_STOP;
		
	return 0;
}

MpdTag * mp3_tagDup(char * file) {
	MpdTag * ret = NULL;
	int time;

	ret = id3Dup(file);

	time = getMp3TotalTime(file);

	if(time>=0) {
		if(!ret) ret = newMpdTag();
		ret->time = time;
	}

	return ret;
}

char * mp3_suffixes[] = {"mp3", NULL};
char * mp3_mimeTypes[] = {"audio/mpeg", NULL};

InputPlugin mp3Plugin = 
{
	"mp3",
	NULL,
	NULL,
	mp3_decode,
	NULL,
	mp3_tagDup,
	INPUT_PLUGIN_STREAM_FILE | INPUT_PLUGIN_STREAM_URL,
	mp3_suffixes,
	mp3_mimeTypes
};
#else

InputPlugin mp3Plugin = 
{
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	0,
	NULL,
	NULL
};

#endif
