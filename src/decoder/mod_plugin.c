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

#include "../decoder_api.h"
#include "../utils.h"
#include "../log.h"

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

static char drv_name[] = "MPD";
static char drv_version[] = "MPD Output Driver v0.1";

#if (LIBMIKMOD_VERSION > 0x030106)
static char drv_alias[] = "mpd";
#endif

static MDRIVER drv_mpd = {
	NULL,
	drv_name,
	drv_version,
	0,
	255,
#if (LIBMIKMOD_VERSION > 0x030106)
	drv_alias,
#if (LIBMIKMOD_VERSION >= 0x030200)
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

static bool mod_mikModInitiated;
static bool mod_mikModInitError;

static bool mod_initMikMod(void)
{
	static char params[] = "";

	if (mod_mikModInitError)
		return false;

	if (!mod_mikModInitiated) {
		mod_mikModInitiated = true;

		md_device = 0;
		md_reverb = 0;

		MikMod_RegisterDriver(&drv_mpd);
		MikMod_RegisterAllLoaders();
	}

	md_pansep = 64;
	md_mixfreq = 44100;
	md_mode = (DMODE_SOFT_MUSIC | DMODE_INTERP | DMODE_STEREO |
		   DMODE_16BITS);

	if (MikMod_Init(params)) {
		ERROR("Could not init MikMod: %s\n",
		      MikMod_strerror(MikMod_errno));
		mod_mikModInitError = true;
		return false;
	}

	return true;
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

static bool
mod_decode(struct decoder *decoder, char *path)
{
	mod_Data *data;
	struct audio_format audio_format;
	float total_time = 0.0;
	int ret;
	float secPerByte;

	if (!mod_initMikMod())
		return false;

	if (!(data = mod_open(path))) {
		ERROR("failed to open mod: %s\n", path);
		MikMod_Exit();
		return false;
	}

	audio_format.bits = 16;
	audio_format.sample_rate = 44100;
	audio_format.channels = 2;

	secPerByte =
	    1.0 / ((audio_format.bits * audio_format.channels / 8.0) *
		   (float)audio_format.sample_rate);

	decoder_initialized(decoder, &audio_format, 0);

	while (true) {
		if (decoder_get_command(decoder) == DECODE_COMMAND_SEEK) {
			decoder_seek_error(decoder);
		}

		if (decoder_get_command(decoder) == DECODE_COMMAND_STOP)
			break;

		if (!Player_Active())
			break;

		ret = VC_WriteBytes(data->audio_buffer, MIKMOD_FRAME_SIZE);
		total_time += ret * secPerByte;
		decoder_data(decoder, NULL, 0,
			     (char *)data->audio_buffer, ret,
			     total_time, 0, NULL);
	}

	mod_close(data);

	MikMod_Exit();

	return true;
}

static struct tag *modTagDup(char *file)
{
	struct tag *ret = NULL;
	MODULE *moduleHandle;
	char *title;

	if (!mod_initMikMod()) {
		DEBUG("modTagDup: Failed to initialize MikMod\n");
		return NULL;
	}

	if (!(moduleHandle = Player_Load(file, 128, 0))) {
		DEBUG("modTagDup: Failed to open file: %s\n", file);
		MikMod_Exit();
		return NULL;

	}
	Player_Free(moduleHandle);

	ret = tag_new();

	ret->time = 0;
	title = xstrdup(Player_LoadTitle(file));
	if (title)
		tag_add_item(ret, TAG_ITEM_TITLE, title);

	MikMod_Exit();

	return ret;
}

static const char *modSuffixes[] = { "amf",
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

struct decoder_plugin modPlugin = {
	.name = "mod",
	.finish = mod_finishMikMod,
	.file_decode = mod_decode,
	.tag_dup = modTagDup,
	.stream_types = INPUT_PLUGIN_STREAM_FILE,
	.suffixes = modSuffixes,
};
