/* the Music Player Daemon (MPD)
 * (c)2003-2005 by Warren Dukes (shank@mercury.chem.pitt.edu)
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

#ifdef HAVE_MPC

#include "../utils.h"
#include "../audio.h"
#include "../log.h"
#include "../pcm_utils.h"
#include "../inputStream.h"
#include "../outputBuffer.h"

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <musepack/musepack.h>
#include <errno.h>

typedef struct _MpcCallbackData {
        InputStream * inStream;
        DecoderControl * dc;
} MpcCallbackData;

/* this is just for tag parsing for db import! */
int getMpcTotalTime(char * file) {
	int totalTime = 0
	
	return totalTime;
}

mpc_int32_t mpc_read_cb(void * vdata, void * ptr, size_t size) {
	mpc_int32_t ret = 0;
        OggCallbackData * data = (OggCallbackData *)vdata;

        while(1) {
	        ret = readFromInputStream(data->inStream, ptr, size, 1);
                if(ret == 0 && !inputStreamAtEOF(data->inStream) && 
                                !data->dc->stop) 
                {
                        my_usleep(10000);
                }
                else break;
        }

	return ret;
}

static mpc_bool_t ogg_seek_cb(void * vdata, ogg_int64_t offset) {
        OggCallbackData * data = (OggCallbackData *)vdata;

	return data->inStream->seekable ? 
		!seekInputStream(data->inStream , offset, SEEK_SET) :
		false;
}

mpd_int32_t mpc_tell_cb(void * vdata) {
        OggCallbackData * data = (OggCallbackData *)vdata;

	return (long)(data->inStream->offset);
}

mpc_bool_t mpc_canseek_cb(void * vdata) {
        OggCallbackData * data = (OggCallbackData *)vdata;

	return data->inStream->seekable;
}

mpc_int32_t mpc_getsize_cb(void * vdata) {
        OggCallbackData * data = (OggCallbackData *)vdata;

	return data->inStream->size;
}

int mpc_decode(OutputBuffer * cb, DecoderControl * dc, InputStream * inStream)
{
	mpc_decoder decoder;
	mpc_reader reader;
	mpc_streaminfo info;

	MpcCallbackData data;
	data.inStream = inStream;
	data.dc = dc;

	int eof = 0;
	long ret;
#define MPC_CHUNK_SIZE 4096
	char chunk[MPC_CHUNK_SIZE];
	int chunkpos = 0;
	long bitRate = 0;
	char ** comments;

        data.inStream = inStream;
        data.dc = dc;

	reader.read = mpc_read_cb;
	reader.seek = mpc_seek_cb;
	reader.tell = mpc_tell_cb;
	reader.get_size = mpc_getsize_cb;
	reader.canseek = mpc_canseek_cb;
	reader.data = &data;

	mpc_streaminfo_init(&info);
	
	if(mpc_streaminfo_read(&info, &reader) != ERROR_CODE_OK) {
		closeInputStream(inStream);
		if(!dc->stop) {
		        ERROR("Not a valid musepack stream");
                }
                else {
                        dc->state = DECODE_STATE_STOP;
                        dc->stop = 0;
                }
                return 0;
	}

	mpc_decoder_setup(&decoder, &reader);
	if(!mpc_decoder_initialize(&decoder, &info)) {
		closeInputStream(inStream);
		if(!dc->stop) {
		        ERROR("Not a valid musepack stream");
                }
                else {
                        dc->state = DECODE_STATE_STOP;
                        dc->stop = 0;
                }
	}
	
	dc->totalTime = 0;

	dc->audioFormat.bits = 16;

	while(!eof) {
		if(dc->seek) {
			if(0 == ov_time_seek_page(&vf,dc->seekWhere)) {
                                clearOutputBuffer(cb);
			        chunkpos = 0;
                        }
                        else dc->seekError = 1;
			dc->seek = 0;
		}
		ret = ov_read(&vf, chunk+chunkpos, 
				OGG_CHUNK_SIZE-chunkpos,
				OGG_DECODE_USE_BIGENDIAN,
				2, 1, &current_section);

		if(current_section!=prev_section) {
			/*printf("new song!\n");*/
			vorbis_info *vi=ov_info(&vf,-1);
			dc->audioFormat.channels = vi->channels;
			dc->audioFormat.sampleRate = vi->rate;
			if(dc->state == DECODE_STATE_START) {
        			getOutputAudioFormat(&(dc->audioFormat),
					&(cb->audioFormat));
				dc->state = DECODE_STATE_DECODE;
			}
			comments = ov_comment(&vf, -1)->user_comments;
			putOggCommentsIntoOutputBuffer(cb, inStream->metaName,
					comments);
        		ogg_getReplayGainInfo(comments, &replayGainInfo);
		}

		prev_section = current_section;

		if(ret <= 0 && ret != OV_HOLE) {
			eof = 1;
			break;
		}
                if(ret == OV_HOLE) ret = 0;

		chunkpos+=ret;

		if(chunkpos >= OGG_CHUNK_SIZE) {
			if((test = ov_bitrate_instant(&vf))>0) {
				bitRate = test/1000;
			}
			sendDataToOutputBuffer(cb, inStream, dc, 
						inStream->seekable,  
                                        	chunk, chunkpos, 
						ov_pcm_tell(&vf)/
						dc->audioFormat.sampleRate,
						bitRate,
						replayGainInfo);
			chunkpos = 0;
			if(dc->stop) break;
		}
	}

	if(!dc->stop && chunkpos > 0) {
		sendDataToOutputBuffer(cb, NULL, dc, inStream->seekable,
				chunk, chunkpos,
				ov_time_tell(&vf), bitRate, replayGainInfo);
	}

	if(replayGainInfo) freeReplayGainInfo(replayGainInfo);

	ov_clear(&vf);

	flushOutputBuffer(cb);

	if(dc->stop) {
		dc->state = DECODE_STATE_STOP;
		dc->stop = 0;
	}
	else dc->state = DECODE_STATE_STOP;

	return 0;
}

MpdTag * oggTagDup(char * file) {
	MpdTag * ret = NULL;
	FILE * fp;
	OggVorbis_File vf;

	fp = fopen(file,"r"); 
	if(!fp) return NULL;
	if(ov_open(fp,&vf,NULL,0)<0) {
		fclose(fp);
		return NULL;
	}

	ret = oggCommentsParse(ov_comment(&vf,-1)->user_comments);

	if(!ret) ret = newMpdTag();
	ret->time = (int)(ov_time_total(&vf,-1)+0.5);

	ov_clear(&vf);

	return ret;	
}

char * oggSuffixes[] = {"ogg", NULL};
char * oggMimeTypes[] = {"application/ogg", NULL};

InputPlugin oggPlugin =
{
        "ogg",
	NULL,
	NULL,
        ogg_decode,
        NULL,
        oggTagDup,
        INPUT_PLUGIN_STREAM_URL | INPUT_PLUGIN_STREAM_FILE,
        oggSuffixes,
        oggMimeTypes
};

#else

InputPlugin oggPlugin = 
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
