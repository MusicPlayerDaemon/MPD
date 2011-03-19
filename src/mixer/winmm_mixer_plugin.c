/*
 * Copyright (C) 2003-2011 The Music Player Daemon Project
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

#include "config.h"
#include "mixer_api.h"
#include "output_api.h"
#include "output/winmm_output_plugin.h"

#include <assert.h>
#include <math.h>
#include <windows.h>

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "winmm_mixer"

struct winmm_mixer {
	struct mixer base;
	struct winmm_output *output;
};

static inline GQuark
winmm_mixer_quark(void)
{
	return g_quark_from_static_string("winmm_mixer");
}

static inline int
winmm_volume_decode(DWORD volume)
{
	return lround((volume & 0xFFFF) / 655.35);
}

static inline DWORD
winmm_volume_encode(int volume)
{
	int value = lround(volume * 655.35);
	return MAKELONG(value, value);
}

static struct mixer *
winmm_mixer_init(void *ao, G_GNUC_UNUSED const struct config_param *param,
		 G_GNUC_UNUSED GError **error_r)
{
	assert(ao != NULL);

	struct winmm_mixer *wm = g_new(struct winmm_mixer, 1);
	mixer_init(&wm->base, &winmm_mixer_plugin);
	wm->output = (struct winmm_output *) ao;

	return &wm->base;
}

static void
winmm_mixer_finish(struct mixer *data)
{
	g_free(data);
}

static int
winmm_mixer_get_volume(struct mixer *mixer, GError **error_r)
{
	struct winmm_mixer *wm = (struct winmm_mixer *) mixer;
	DWORD volume;
	HWAVEOUT handle = winmm_output_get_handle(wm->output);
	MMRESULT result = waveOutGetVolume(handle, &volume);

	if (result != MMSYSERR_NOERROR) {
		g_set_error(error_r, 0, winmm_mixer_quark(),
			    "Failed to get winmm volume");
		return -1;
	}

	return winmm_volume_decode(volume);
}

static bool
winmm_mixer_set_volume(struct mixer *mixer, unsigned volume, GError **error_r)
{
	struct winmm_mixer *wm = (struct winmm_mixer *) mixer;
	DWORD value = winmm_volume_encode(volume);
	HWAVEOUT handle = winmm_output_get_handle(wm->output);
	MMRESULT result = waveOutSetVolume(handle, value);

	if (result != MMSYSERR_NOERROR) {
		g_set_error(error_r, 0, winmm_mixer_quark(),
			    "Failed to set winmm volume");
		return false;
	}

	return true;
}

const struct mixer_plugin winmm_mixer_plugin = {
	.init = winmm_mixer_init,
	.finish = winmm_mixer_finish,
	.get_volume = winmm_mixer_get_volume,
	.set_volume = winmm_mixer_set_volume,
};
