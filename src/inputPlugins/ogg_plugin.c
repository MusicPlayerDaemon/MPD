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

#ifdef HAVE_OGG

#include "../command.h"
#include "../utils.h"
#include "../audio.h"
#include "../log.h"
#include "../pcm_utils.h"
#include "../inputStream.h"
#include "../outputBuffer.h"
#include "../replayGain.h"

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <vorbis/vorbisfile.h>
#include <errno.h>

#ifdef WORDS_BIGENDIAN
#define OGG_DECODE_USE_BIGENDIAN	1
#else
#define OGG_DECODE_USE_BIGENDIAN	0
#endif

typedef struct _OggCallbackData {
        InputStream * inStream;
        DecoderControl * dc;
} OggCallbackData;

/* this is just for tag parsing for db import! */
int getOggTotalTime(char * file) {
	OggVorbis_File vf;
	FILE * oggfp;
	int totalTime;
	
	if(!(oggfp = fopen(file,"r"))) return -1;
		
	if(ov_open(oggfp, &vf, NULL, 0) < 0) {
		fclose(oggfp);
		return -1;
	}
	
	totalTime = ov_time_total(&vf,-1)+0.5;

	ov_clear(&vf);

	return totalTime;
}

size_t ogg_read_cb(void * ptr, size_t size, size_t nmemb, void * vdata)
{
	size_t ret = 0;
        OggCallbackData * data = (OggCallbackData *)vdata;

        while(1) {
	        ret = readFromInputStream(data->inStream,ptr,size,nmemb);
                if(ret == 0 && !inputStreamAtEOF(data->inStream) && 
                                !data->dc->stop) 
                {
                        my_usleep(10000);
                }
                else break;
        }
        errno = 0;
	/*if(ret<0) errno = ((InputStream *)inStream)->error;*/

	return ret;
}

int ogg_seek_cb(void * vdata, ogg_int64_t offset, int whence) {
        OggCallbackData * data = (OggCallbackData *)vdata;

	return seekInputStream(data->inStream,offset,whence);
}

int ogg_close_cb(void * vdata) {
        OggCallbackData * data = (OggCallbackData *)vdata;

	return closeInputStream(data->inStream);
}

long ogg_tell_cb(void * vdata) {
        OggCallbackData * data = (OggCallbackData *)vdata;

	return (long)(data->inStream->offset);
}

char * ogg_parseComment(char * comment, char * needle) {
        int len = strlen(needle);

        if(strncasecmp(comment, needle, len) == 0 && *(comment+len) == '=') {
		return comment+len+1;
	}

        return NULL;
}

float ogg_getReplayGainScale(char ** comments) {
        int trackGainFound = 0;
        int albumGainFound = 0;
        float trackGain = 1.0;
        float albumGain = 1.0;
        float trackPeak = 0.0;
        float albumPeak = 0.0;
        char * temp;
        int replayGainState = getReplayGainState();

        if(replayGainState == REPLAYGAIN_OFF) return 1.0;

        while(*comments) {
                if((temp = ogg_parseComment(*comments,"replaygain_track_gain"))) 
                {
                        trackGain = atof(temp);
                        trackGainFound = 1;
                }
                else if((temp = ogg_parseComment(*comments,
                                        "replaygain_album_gain"))) 
                {
                        albumGain = atof(temp);
                        albumGainFound = 1;
                }
                else if((temp = ogg_parseComment(*comments,
                                        "replaygain_track_peak"))) 
                {
                        trackPeak = atof(temp);
                }
                else if((temp = ogg_parseComment(*comments,
                                        "replaygain_album_peak"))) 
                {
                        albumPeak = atof(temp);
                }

                comments++;
        }

        switch(replayGainState) {
        case REPLAYGAIN_ALBUM:
                if(albumGainFound) {
                        return computeReplayGainScale(albumGain,albumPeak);
                }
        default:
                return computeReplayGainScale(trackGain,trackPeak);
        }

        return 1.0;
}

MpdTag * oggCommentsParse(char ** comments) {
	MpdTag * ret = NULL;
	char * temp;

	while(*comments) {
                if((temp = ogg_parseComment(*comments,"artist"))) {
			if(!ret) ret = newMpdTag();
			if(!ret->artist) {
				ret->artist = strdup(temp);
			}
		} 
                else if((temp = ogg_parseComment(*comments,"title"))) {
			if(!ret) ret = newMpdTag();
			if(!ret->title) {
				ret->title = strdup(temp);
			}
		}
                else if((temp = ogg_parseComment(*comments,"album"))) {
			if(!ret) ret = newMpdTag();
			if(!ret->album) {
				ret->album = strdup(temp);
			}
		}
                else if((temp = ogg_parseComment(*comments,"tracknumber"))) {
			if(!ret) ret = newMpdTag();
			if(!ret->track) {
				ret->track = strdup(temp);
			}
		}

		comments++;
	}

	return ret;
}

void putOggCommentsIntoDecoderControlMetadata(DecoderControl * dc,
		char ** comments)
{
	MpdTag * tag;

	if(dc->metadataSet) return;

	tag = oggCommentsParse(comments);
	if(!tag) return;

	copyMpdTagToDecoderControlMetadata(dc, tag);

	freeMpdTag(tag);
}

int ogg_decode(OutputBuffer * cb, DecoderControl * dc, InputStream * inStream)
{
	OggVorbis_File vf;
	ov_callbacks callbacks;
        OggCallbackData data;
	int current_section;
	int eof = 0;
	long ret;
#define OGG_CHUNK_SIZE 4096
	char chunk[OGG_CHUNK_SIZE];
	int chunkpos = 0;
	long bitRate = 0;
	long test;
        float replayGainScale;
	char ** comments;

        data.inStream = inStream;
        data.dc = dc;

	callbacks.read_func = ogg_read_cb;
	callbacks.seek_func = ogg_seek_cb;
	callbacks.close_func = ogg_close_cb;
	callbacks.tell_func = ogg_tell_cb;
	
	if(ov_open_callbacks(&data, &vf, NULL, 0, callbacks) < 0) {
		closeInputStream(inStream);
		if(!dc->stop) {
		        ERROR("Input does not appear to be an Ogg Vorbis stream.\n");
                        return -1;
                }
                else {
                        dc->state = DECODE_STATE_STOP;
                        dc->stop = 0;
                }
                return 0;
	}
	
	{
		vorbis_info *vi=ov_info(&vf,-1);
		dc->audioFormat.bits = 16;
		dc->audioFormat.channels = vi->channels;
		dc->audioFormat.sampleRate = vi->rate;
                getOutputAudioFormat(&(dc->audioFormat),&(cb->audioFormat));
	}

	dc->totalTime = ov_time_total(&vf,-1);
        if(dc->totalTime < 0) dc->totalTime = 0;

	comments = ov_comment(&vf, -1)->user_comments;

	putOggCommentsIntoDecoderControlMetadata(dc, comments);

	dc->state = DECODE_STATE_DECODE;

        replayGainScale = ogg_getReplayGainScale(comments);

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
                	doReplayGain(chunk,ret,&(dc->audioFormat), 
					replayGainScale);
			sendDataToOutputBuffer(cb, inStream, dc, 
						inStream->seekable,  
                                        	chunk, chunkpos, 
						ov_time_tell(&vf), 
						bitRate);
					
			if(dc->stop) break;
			chunkpos = 0;
		}
	}

	if(!dc->stop && chunkpos > 0) {
		sendDataToOutputBuffer(cb, NULL, dc, inStream->seekable,
				chunk, chunkpos,
				ov_time_tell(&vf), bitRate);
	}

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
