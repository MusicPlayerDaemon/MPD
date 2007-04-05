/* the Music Player Daemon (MPD)
 * Copyright (C) 2003-2007 by Warren Dukes (warren.dukes@gmail.com)
 * This project's homepage is: http://www.musicpd.org
 * 
 * libaudiofile (wave) support added by Eric Wong <normalperson@yhbt.net>
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

#ifdef HAVE_AUDIOFILE

#include "../utils.h"
#include "../audio.h"
#include "../log.h"
#include "../pcm_utils.h"
#include "../playerData.h"

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <audiofile.h>

static int getAudiofileTotalTime(char *file)
{
	int time;
	AFfilehandle af_fp = afOpenFile(file, "r", NULL);
	if (af_fp == AF_NULL_FILEHANDLE) {
		return -1;
	}
	time = (int)
	    ((double)afGetFrameCount(af_fp, AF_DEFAULT_TRACK)
	     / afGetRate(af_fp, AF_DEFAULT_TRACK));
	afCloseFile(af_fp);
	return time;
}

static int audiofile_decode(OutputBuffer * cb, DecoderControl * dc, char *path)
{
	int fs, frame_count;
	AFfilehandle af_fp;
	int bits;
	mpd_uint16 bitRate;
	struct stat st;

	if (stat(path, &st) < 0) {
		ERROR("failed to stat: %s\n", path);
		return -1;
	}

	af_fp = afOpenFile(path, "r", NULL);
	if (af_fp == AF_NULL_FILEHANDLE) {
		ERROR("failed to open: %s\n", path);
		return -1;
	}

	afSetVirtualSampleFormat(af_fp, AF_DEFAULT_TRACK,
	                         AF_SAMPFMT_TWOSCOMP, 16);
	afGetVirtualSampleFormat(af_fp, AF_DEFAULT_TRACK, &fs, &bits);
	dc->audioFormat.bits = bits;
	dc->audioFormat.sampleRate = afGetRate(af_fp, AF_DEFAULT_TRACK);
	dc->audioFormat.channels = afGetVirtualChannels(af_fp, AF_DEFAULT_TRACK);
	getOutputAudioFormat(&(dc->audioFormat), &(cb->audioFormat));

	frame_count = afGetFrameCount(af_fp, AF_DEFAULT_TRACK);

	dc->totalTime =
	    ((float)frame_count / (float)dc->audioFormat.sampleRate);

	bitRate = st.st_size * 8.0 / dc->totalTime / 1000.0 + 0.5;

	if (dc->audioFormat.bits != 8 && dc->audioFormat.bits != 16) {
		ERROR("Only 8 and 16-bit files are supported. %s is %i-bit\n",
		      path, dc->audioFormat.bits);
		afCloseFile(af_fp);
		return -1;
	}

	fs = (int)afGetVirtualFrameSize(af_fp, AF_DEFAULT_TRACK, 1);

	dc->state = DECODE_STATE_DECODE;
	{
		int ret, eof = 0, current = 0;
		char chunk[CHUNK_SIZE];

		while (!eof) {
			if (dc->seek) {
				clearOutputBuffer(cb);
				current = dc->seekWhere *
				    dc->audioFormat.sampleRate;
				afSeekFrame(af_fp, AF_DEFAULT_TRACK, current);
				dc->seek = 0;
			}

			ret =
			    afReadFrames(af_fp, AF_DEFAULT_TRACK, chunk,
					 CHUNK_SIZE / fs);
			if (ret <= 0)
				eof = 1;
			else {
				current += ret;
				sendDataToOutputBuffer(cb,
						       NULL,
						       dc,
						       1,
						       chunk,
						       ret * fs,
						       (float)current /
						       (float)dc->audioFormat.
						       sampleRate, bitRate,
						       NULL);
				if (dc->stop)
					break;
			}
		}

		flushOutputBuffer(cb);

		/*if(dc->seek) {
		   dc->seekError = 1;
		   dc->seek = 0;
		   } */

		if (dc->stop) {
			dc->state = DECODE_STATE_STOP;
			dc->stop = 0;
		} else
			dc->state = DECODE_STATE_STOP;
	}
	afCloseFile(af_fp);

	return 0;
}

static MpdTag *audiofileTagDup(char *file)
{
	MpdTag *ret = NULL;
	int time = getAudiofileTotalTime(file);

	if (time >= 0) {
		if (!ret)
			ret = newMpdTag();
		ret->time = time;
	} else {
		DEBUG
		    ("audiofileTagDup: Failed to get total song time from: %s\n",
		     file);
	}

	return ret;
}

static char *audiofileSuffixes[] = { "wav", "au", "aiff", "aif", NULL };

InputPlugin audiofilePlugin = {
	"audiofile",
	NULL,
	NULL,
	NULL,
	NULL,
	audiofile_decode,
	audiofileTagDup,
	INPUT_PLUGIN_STREAM_FILE,
	audiofileSuffixes,
	NULL
};

#else

InputPlugin audiofilePlugin;

#endif /* HAVE_AUDIOFILE */
