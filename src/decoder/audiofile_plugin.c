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
#include "../log.h"

#include <sys/stat.h>
#include <audiofile.h>

/* pick 1020 since its devisible for 8,16,24, and 32-bit audio */
#define CHUNK_SIZE		1020

static int getAudiofileTotalTime(const char *file)
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

static void
audiofile_decode(struct decoder *decoder, const char *path)
{
	int fs, frame_count;
	AFfilehandle af_fp;
	int bits;
	struct audio_format audio_format;
	float total_time;
	uint16_t bitRate;
	struct stat st;
	int ret, current = 0;
	char chunk[CHUNK_SIZE];

	if (stat(path, &st) < 0) {
		ERROR("failed to stat: %s\n", path);
		return;
	}

	af_fp = afOpenFile(path, "r", NULL);
	if (af_fp == AF_NULL_FILEHANDLE) {
		ERROR("failed to open: %s\n", path);
		return;
	}

	afSetVirtualSampleFormat(af_fp, AF_DEFAULT_TRACK,
	                         AF_SAMPFMT_TWOSCOMP, 16);
	afGetVirtualSampleFormat(af_fp, AF_DEFAULT_TRACK, &fs, &bits);
	audio_format.bits = (uint8_t)bits;
	audio_format.sample_rate =
	                      (unsigned int)afGetRate(af_fp, AF_DEFAULT_TRACK);
	audio_format.channels =
	              (uint8_t)afGetVirtualChannels(af_fp, AF_DEFAULT_TRACK);

	frame_count = afGetFrameCount(af_fp, AF_DEFAULT_TRACK);

	total_time = ((float)frame_count / (float)audio_format.sample_rate);

	bitRate = (uint16_t)(st.st_size * 8.0 / total_time / 1000.0 + 0.5);

	if (audio_format.bits != 8 && audio_format.bits != 16) {
		ERROR("Only 8 and 16-bit files are supported. %s is %i-bit\n",
		      path, audio_format.bits);
		afCloseFile(af_fp);
		return;
	}

	fs = (int)afGetVirtualFrameSize(af_fp, AF_DEFAULT_TRACK, 1);

	decoder_initialized(decoder, &audio_format, true, total_time);

	do {
		if (decoder_get_command(decoder) == DECODE_COMMAND_SEEK) {
			current = decoder_seek_where(decoder) *
				audio_format.sample_rate;
			afSeekFrame(af_fp, AF_DEFAULT_TRACK, current);
			decoder_command_finished(decoder);
		}

		ret = afReadFrames(af_fp, AF_DEFAULT_TRACK, chunk,
				   CHUNK_SIZE / fs);
		if (ret <= 0)
			break;

		current += ret;
		decoder_data(decoder, NULL,
			     chunk, ret * fs,
			     (float)current / (float)audio_format.sample_rate,
			     bitRate, NULL);
	} while (decoder_get_command(decoder) != DECODE_COMMAND_STOP);

	afCloseFile(af_fp);
}

static struct tag *audiofileTagDup(const char *file)
{
	struct tag *ret = NULL;
	int total_time = getAudiofileTotalTime(file);

	if (total_time >= 0) {
		if (!ret)
			ret = tag_new();
		ret->time = total_time;
	} else {
		DEBUG
		    ("audiofileTagDup: Failed to get total song time from: %s\n",
		     file);
	}

	return ret;
}

static const char *const audiofileSuffixes[] = {
	"wav", "au", "aiff", "aif", NULL
};

const struct decoder_plugin audiofilePlugin = {
	.name = "audiofile",
	.file_decode = audiofile_decode,
	.tag_dup = audiofileTagDup,
	.suffixes = audiofileSuffixes,
};
