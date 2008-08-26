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

#include "../decoder_api.h"

#ifdef HAVE_AUDIOFILE

#include "../log.h"

#include <audiofile.h>

static int getAudiofileTotalTime(char *file)
{
	int total_time;
	AFfilehandle af_fp = afOpenFile(file, "r", NULL);
	if (af_fp == AF_NULL_FILEHANDLE) {
		return -1;
	}
	total_time = (int)
	    ((double)afGetFrameCount(af_fp, AF_DEFAULT_TRACK)
	     / afGetRate(af_fp, AF_DEFAULT_TRACK));
	afCloseFile(af_fp);
	return total_time;
}

static int audiofile_decode(struct decoder * decoder, char *path)
{
	int fs, frame_count;
	AFfilehandle af_fp;
	int bits;
	AudioFormat audio_format;
	float total_time;
	mpd_uint16 bitRate;
	struct stat st;
	int ret, current = 0;
	char chunk[CHUNK_SIZE];

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
	audio_format.bits = (mpd_uint8)bits;
	audio_format.sampleRate =
	                      (unsigned int)afGetRate(af_fp, AF_DEFAULT_TRACK);
	audio_format.channels =
	              (mpd_uint8)afGetVirtualChannels(af_fp, AF_DEFAULT_TRACK);

	frame_count = afGetFrameCount(af_fp, AF_DEFAULT_TRACK);

	total_time = ((float)frame_count / (float)audio_format.sampleRate);

	bitRate = (mpd_uint16)(st.st_size * 8.0 / total_time / 1000.0 + 0.5);

	if (audio_format.bits != 8 && audio_format.bits != 16) {
		ERROR("Only 8 and 16-bit files are supported. %s is %i-bit\n",
		      path, audio_format.bits);
		afCloseFile(af_fp);
		return -1;
	}

	fs = (int)afGetVirtualFrameSize(af_fp, AF_DEFAULT_TRACK, 1);

	decoder_initialized(decoder, &audio_format, total_time);

	do {
		if (decoder_get_command(decoder) == DECODE_COMMAND_SEEK) {
			decoder_clear(decoder);
			current = decoder_seek_where(decoder) *
				audio_format.sampleRate;
			afSeekFrame(af_fp, AF_DEFAULT_TRACK, current);
			decoder_command_finished(decoder);
		}

		ret = afReadFrames(af_fp, AF_DEFAULT_TRACK, chunk,
				   CHUNK_SIZE / fs);
		if (ret <= 0)
			break;

		current += ret;
		decoder_data(decoder, NULL, 1,
			     chunk, ret * fs,
			     (float)current / (float)audio_format.sampleRate,
			     bitRate, NULL);
	} while (decoder_get_command(decoder) != DECODE_COMMAND_STOP);

	decoder_flush(decoder);

	afCloseFile(af_fp);

	return 0;
}

static MpdTag *audiofileTagDup(char *file)
{
	MpdTag *ret = NULL;
	int total_time = getAudiofileTotalTime(file);

	if (total_time >= 0) {
		if (!ret)
			ret = newMpdTag();
		ret->time = total_time;
	} else {
		DEBUG
		    ("audiofileTagDup: Failed to get total song time from: %s\n",
		     file);
	}

	return ret;
}

static const char *audiofileSuffixes[] = { "wav", "au", "aiff", "aif", NULL };

struct decoder_plugin audiofilePlugin = {
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

struct decoder_plugin audiofilePlugin;

#endif /* HAVE_AUDIOFILE */
