/* the Music Player Daemon (MPD)
 * Copyright (C) 2003-2007 by Warren Dukes (warren.dukes@gmail.com)
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

#ifdef HAVE_MIKMOD

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
#include <mikmod.h>

/* this is largely copied from alsaplayer */

#define MIKMOD_FRAME_SIZE	4096

static BOOL mod_mpd_Init(void)
{
	return VC_Init();
}

static void mod_mpd_Exit(void)
{
	VC_Exit();
}

static void mod_mpd_Update(void)
{
}

static BOOL mod_mpd_IsThere(void)
{
	return 1;
}

static MDRIVER drv_mpd = {
	NULL,
	"MPD",
	"MPD Output Driver v0.1",
	0,
	255,
#if (LIBMIKMOD_VERSION > 0x030106)
	"mpd", /* Alias */
#if (LIBMIKMOD_VERSION > 0x030200)
	NULL,  /* CmdLineHelp */
#endif
	NULL,  /* CommandLine */
#endif
	mod_mpd_IsThere,
	VC_SampleLoad,
	VC_SampleUnload,
	VC_SampleSpace,
	VC_SampleLength,
	mod_mpd_Init,
	mod_mpd_Exit,
	NULL,
	VC_SetNumVoices,
	VC_PlayStart,
	VC_PlayStop,
	mod_mpd_Update,
	NULL,
	VC_VoiceSetVolume,
	VC_VoiceGetVolume,
	VC_VoiceSetFrequency,
	VC_VoiceGetFrequency,
	VC_VoiceSetPanning,
	VC_VoiceGetPanning,
	VC_VoicePlay,
	VC_VoiceStop,
	VC_VoiceStopped,
	VC_VoiceGetPosition,
	VC_VoiceRealVolume
};

static int mod_mikModInitiated;
static int mod_mikModInitError;

static int mod_initMikMod(void)
{
	if (mod_mikModInitError)
		return -1;

	if (!mod_mikModInitiated) {
		mod_mikModInitiated = 1;

		md_device = 0;
		md_reverb = 0;

		MikMod_RegisterDriver(&drv_mpd);
		MikMod_RegisterAllLoaders();
	}

	md_pansep = 64;
	md_mixfreq = 44100;
	md_mode = (DMODE_SOFT_MUSIC | DMODE_INTERP | DMODE_STEREO |
		   DMODE_16BITS);

	if (MikMod_Init("")) {
		ERROR("Could not init MikMod: %s\n",
		      MikMod_strerror(MikMod_errno));
		mod_mikModInitError = 1;
		return -1;
	}

	return 0;
}

static void mod_finishMikMod(void)
{
	MikMod_Exit();
}

typedef struct _mod_Data {
	MODULE *moduleHandle;
	SBYTE *audio_buffer;
} mod_Data;

static mod_Data *mod_open(char *path)
{
	MODULE *moduleHandle;
	mod_Data *data;

	if (!(moduleHandle = Player_Load(path, 128, 0)))
		return NULL;

	/* Prevent module from looping forever */
	moduleHandle->loop = 0;

	data = xmalloc(sizeof(mod_Data));

	data->audio_buffer = xmalloc(MIKMOD_FRAME_SIZE);
	data->moduleHandle = moduleHandle;

	Player_Start(data->moduleHandle);

	return data;
}

static void mod_close(mod_Data * data)
{
	Player_Stop();
	Player_Free(data->moduleHandle);
	free(data->audio_buffer);
	free(data);
}

static int mod_decode(OutputBuffer * cb, DecoderControl * dc, char *path)
{
	mod_Data *data;
	float time = 0.0;
	int ret;
	float secPerByte;

	if (mod_initMikMod() < 0)
		return -1;

	if (!(data = mod_open(path))) {
		ERROR("failed to open mod: %s\n", path);
		MikMod_Exit();
		return -1;
	}

	dc->totalTime = 0;
	dc->audioFormat.bits = 16;
	dc->audioFormat.sampleRate = 44100;
	dc->audioFormat.channels = 2;
	getOutputAudioFormat(&(dc->audioFormat), &(cb->audioFormat));

	secPerByte =
	    1.0 / ((dc->audioFormat.bits * dc->audioFormat.channels / 8.0) *
		   (float)dc->audioFormat.sampleRate);

	dc->state = DECODE_STATE_DECODE;
	while (1) {
		if (dc->seek) {
			dc->seekError = 1;
			dc->seek = 0;
		}

		if (dc->stop)
			break;

		if (!Player_Active())
			break;

		ret = VC_WriteBytes(data->audio_buffer, MIKMOD_FRAME_SIZE);
		time += ret * secPerByte;
		sendDataToOutputBuffer(cb, NULL, dc, 0,
				       (char *)data->audio_buffer, ret, time,
				       0, NULL);
	}

	flushOutputBuffer(cb);

	mod_close(data);

	MikMod_Exit();

	if (dc->stop) {
		dc->state = DECODE_STATE_STOP;
		dc->stop = 0;
	} else
		dc->state = DECODE_STATE_STOP;

	return 0;
}

static MpdTag *modTagDup(char *file)
{
	MpdTag *ret = NULL;
	MODULE *moduleHandle;
	char *title;

	if (mod_initMikMod() < 0) {
		DEBUG("modTagDup: Failed to initialize MikMod\n");
		return NULL;
	}

	if (!(moduleHandle = Player_Load(file, 128, 0))) {
		DEBUG("modTagDup: Failed to open file: %s\n", file);
		MikMod_Exit();
		return NULL;

	}
	Player_Free(moduleHandle);

	ret = newMpdTag();

	ret->time = 0;
	title = xstrdup(Player_LoadTitle(file));
	if (title)
		addItemToMpdTag(ret, TAG_ITEM_TITLE, title);

	MikMod_Exit();

	return ret;
}

static char *modSuffixes[] = { "amf",
	"dsm",
	"far",
	"gdm",
	"imf",
	"it",
	"med",
	"mod",
	"mtm",
	"s3m",
	"stm",
	"stx",
	"ult",
	"uni",
	"xm",
	NULL
};

InputPlugin modPlugin = {
	"mod",
	NULL,
	mod_finishMikMod,
	NULL,
	NULL,
	mod_decode,
	modTagDup,
	INPUT_PLUGIN_STREAM_FILE,
	modSuffixes,
	NULL
};

#else

InputPlugin modPlugin;

#endif /* HAVE_MIKMOD */
