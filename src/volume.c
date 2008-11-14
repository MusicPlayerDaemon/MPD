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
#include "volume.h"

#include "conf.h"
#include "log.h"
#include "player_control.h"
#include "utils.h"
#include "idle.h"
#include "pcm_utils.h"
#include "os_compat.h"

#include "../config.h"

#include <glib.h>

#ifdef HAVE_OSS
#include <sys/soundcard.h>
#endif
#ifdef HAVE_ALSA
#include <alsa/asoundlib.h>
#endif

#define VOLUME_MIXER_TYPE_SOFTWARE		0
#define VOLUME_MIXER_TYPE_OSS			1
#define VOLUME_MIXER_TYPE_ALSA			2

#define VOLUME_MIXER_SOFTWARE_DEFAULT		""
#define VOLUME_MIXER_OSS_DEFAULT		"/dev/mixer"
#define VOLUME_MIXER_ALSA_DEFAULT		"default"
#define VOLUME_MIXER_ALSA_CONTROL_DEFAULT	"PCM"
#define SW_VOLUME_STATE                         "sw_volume: "

#ifdef HAVE_OSS
#define VOLUME_MIXER_TYPE_DEFAULT               VOLUME_MIXER_TYPE_OSS
#define VOLUME_MIXER_DEVICE_DEFAULT             VOLUME_MIXER_OSS_DEFAULT
#else
#ifdef HAVE_ALSA
#define VOLUME_MIXER_TYPE_DEFAULT               VOLUME_MIXER_TYPE_ALSA
#define VOLUME_MIXER_DEVICE_DEFAULT             VOLUME_MIXER_ALSA_DEFAULT
#else
#define VOLUME_MIXER_TYPE_DEFAULT               VOLUME_MIXER_TYPE_SOFTWARE
#define VOLUME_MIXER_DEVICE_DEFAULT             VOLUME_MIXER_SOFTWARE_DEFAULT
#endif
#endif

static int volume_mixerType = VOLUME_MIXER_TYPE_DEFAULT;
static const char *volume_mixerDevice = VOLUME_MIXER_DEVICE_DEFAULT;

static int volume_softwareSet = 100;

#ifdef HAVE_OSS
static int volume_ossFd = -1;
static int volume_ossControl = SOUND_MIXER_PCM;
#endif

#ifdef HAVE_ALSA
static snd_mixer_t *volume_alsaMixerHandle;
static snd_mixer_elem_t *volume_alsaElem;
static long volume_alsaMin;
static long volume_alsaMax;
static int volume_alsaSet = -1;
#endif

#ifdef HAVE_OSS

static void closeOssMixer(void)
{
	while (close(volume_ossFd) && errno == EINTR) ;
	volume_ossFd = -1;
}

static int
oss_find_mixer(const char *name)
{
	const char *labels[SOUND_MIXER_NRDEVICES] = SOUND_DEVICE_LABELS;
	size_t name_length = strlen(name);

	for (unsigned i = 0; i < SOUND_MIXER_NRDEVICES; i++) {
		if (strncasecmp(name, labels[i], name_length) == 0 &&
		    (labels[i][name_length] == 0 ||
		     labels[i][name_length] == ' '))
			return i;
	}

	return -1;
}

static int prepOssMixer(const char *device)
{
	ConfigParam *param;

	if ((volume_ossFd = open(device, O_RDONLY)) < 0) {
		WARNING("unable to open oss mixer \"%s\"\n", device);
		return -1;
	}

	if ((param = getConfigParam(CONF_MIXER_CONTROL))) {
		int i;
		int devmask = 0;

		if (ioctl(volume_ossFd, SOUND_MIXER_READ_DEVMASK, &devmask) < 0) {
			WARNING("errors getting read_devmask for oss mixer\n");
			closeOssMixer();
			return -1;
		}

		i = oss_find_mixer(param->value);

		if (i < 0) {
			WARNING("mixer control \"%s\" not found at line %i\n",
				param->value, param->line);
			closeOssMixer();
			return -1;
		} else if (!((1 << i) & devmask)) {
			WARNING("mixer control \"%s\" not usable at line %i\n",
				param->value, param->line);
			closeOssMixer();
			return -1;
		}

		volume_ossControl = i;
	}

	return 0;
}

static int ensure_oss_open(void)
{
	if ((volume_ossFd < 0 && prepOssMixer(volume_mixerDevice) < 0))
		return -1;
	return 0;
}

static int getOssVolumeLevel(void)
{
	int left, right, level;

	if (ensure_oss_open() < 0)
		return -1;

	if (ioctl(volume_ossFd, MIXER_READ(volume_ossControl), &level) < 0) {
		closeOssMixer();
		WARNING("unable to read volume\n");
		return -1;
	}

	left = level & 0xff;
	right = (level & 0xff00) >> 8;

	if (left != right) {
		WARNING("volume for left and right is not the same, \"%i\" and "
			"\"%i\"\n", left, right);
	}

	return left;
}

static int changeOssVolumeLevel(int change, int rel)
{
	int current;
	int new;
	int level;

	if (rel) {
		if ((current = getOssVolumeLevel()) < 0)
			return -1;

		new = current + change;
	} else {
		if (ensure_oss_open() < 0)
			return -1;
		new = change;
	}

	if (new < 0)
		new = 0;
	else if (new > 100)
		new = 100;

	level = (new << 8) + new;

	if (ioctl(volume_ossFd, MIXER_WRITE(volume_ossControl), &level) < 0) {
		closeOssMixer();
		return -1;
	}

	return 0;
}
#endif

#ifdef HAVE_ALSA
static void closeAlsaMixer(void)
{
	snd_mixer_close(volume_alsaMixerHandle);
	volume_alsaMixerHandle = NULL;
}

static int prepAlsaMixer(const char *card)
{
	int err;
	snd_mixer_elem_t *elem;
	const char *controlName = VOLUME_MIXER_ALSA_CONTROL_DEFAULT;
	ConfigParam *param;

	err = snd_mixer_open(&volume_alsaMixerHandle, 0);
	snd_config_update_free_global();
	if (err < 0) {
		WARNING("problems opening alsa mixer: %s\n", snd_strerror(err));
		return -1;
	}

	if ((err = snd_mixer_attach(volume_alsaMixerHandle, card)) < 0) {
		closeAlsaMixer();
		WARNING("problems attaching alsa mixer: %s\n",
			snd_strerror(err));
		return -1;
	}

	if ((err =
	     snd_mixer_selem_register(volume_alsaMixerHandle, NULL,
				      NULL)) < 0) {
		closeAlsaMixer();
		WARNING("problems snd_mixer_selem_register'ing: %s\n",
			snd_strerror(err));
		return -1;
	}

	if ((err = snd_mixer_load(volume_alsaMixerHandle)) < 0) {
		closeAlsaMixer();
		WARNING("problems snd_mixer_selem_register'ing: %s\n",
			snd_strerror(err));
		return -1;
	}

	elem = snd_mixer_first_elem(volume_alsaMixerHandle);

	param = getConfigParam(CONF_MIXER_CONTROL);

	if (param) {
		controlName = param->value;
	}

	while (elem) {
		if (snd_mixer_elem_get_type(elem) == SND_MIXER_ELEM_SIMPLE) {
			if (strcasecmp(controlName,
				       snd_mixer_selem_get_name(elem)) == 0) {
				break;
			}
		}
		elem = snd_mixer_elem_next(elem);
	}

	if (elem) {
		volume_alsaElem = elem;
		snd_mixer_selem_get_playback_volume_range(volume_alsaElem,
							  &volume_alsaMin,
							  &volume_alsaMax);
		return 0;
	}

	WARNING("can't find alsa mixer_control \"%s\"\n", controlName);

	closeAlsaMixer();
	return -1;
}

static int prep_alsa_get_level(long *level)
{
	const char *cmd;
	int err;

	if (!volume_alsaMixerHandle && prepAlsaMixer(volume_mixerDevice) < 0)
		return -1;

	if ((err = snd_mixer_handle_events(volume_alsaMixerHandle)) < 0) {
		cmd = "handle_events";
		goto error;
	}
	if ((err = snd_mixer_selem_get_playback_volume(volume_alsaElem,
						       SND_MIXER_SCHN_FRONT_LEFT,
						       level)) < 0) {
		cmd = "selem_get_playback_volume";
		goto error;
	}
	return 0;

error:
	WARNING("problems getting alsa volume: %s (snd_mixer_%s)\n",
		snd_strerror(err), cmd);
	closeAlsaMixer();
	return -1;
}

static int getAlsaVolumeLevel(void)
{
	int ret;
	long level;
	long max = volume_alsaMax;
	long min = volume_alsaMin;

	if (prep_alsa_get_level(&level) < 0)
		return -1;

	ret = ((volume_alsaSet / 100.0) * (max - min) + min) + 0.5;
	if (volume_alsaSet > 0 && ret == level) {
		ret = volume_alsaSet;
	} else
		ret = (int)(100 * (((float)(level - min)) / (max - min)) + 0.5);

	return ret;
}

static int changeAlsaVolumeLevel(int change, int rel)
{
	float vol;
	long level;
	long test;
	long max = volume_alsaMax;
	long min = volume_alsaMin;
	int err;

	if (prep_alsa_get_level(&level) < 0)
		return -1;

	if (rel) {
		test = ((volume_alsaSet / 100.0) * (max - min) + min) + 0.5;
		if (volume_alsaSet >= 0 && level == test) {
			vol = volume_alsaSet;
		} else
			vol = 100.0 * (((float)(level - min)) / (max - min));
		vol += change;
	} else
		vol = change;

	volume_alsaSet = vol + 0.5;
	volume_alsaSet = volume_alsaSet > 100 ? 100 :
	    (volume_alsaSet < 0 ? 0 : volume_alsaSet);

	level = (long)(((vol / 100.0) * (max - min) + min) + 0.5);
	level = level > max ? max : level;
	level = level < min ? min : level;

	if ((err =
	     snd_mixer_selem_set_playback_volume_all(volume_alsaElem,
						     level)) < 0) {
		WARNING("problems setting alsa volume: %s\n",
			snd_strerror(err));
		closeAlsaMixer();
		return -1;
	}

	return 0;
}
#endif

static int prepMixer(const char *device)
{
	switch (volume_mixerType) {
#ifdef HAVE_ALSA
	case VOLUME_MIXER_TYPE_ALSA:
		return prepAlsaMixer(device);
#endif
#ifdef HAVE_OSS
	case VOLUME_MIXER_TYPE_OSS:
		return prepOssMixer(device);
#endif
	}

	return 0;
}

void finishVolume(void)
{
	switch (volume_mixerType) {
#ifdef HAVE_ALSA
	case VOLUME_MIXER_TYPE_ALSA:
		closeAlsaMixer();
		break;
#endif
#ifdef HAVE_OSS
	case VOLUME_MIXER_TYPE_OSS:
		closeOssMixer();
		break;
#endif
	}
}

void initVolume(void)
{
	ConfigParam *param = getConfigParam(CONF_MIXER_TYPE);

	if (param) {
		if (0) ;
#ifdef HAVE_ALSA
		else if (strcmp(param->value, VOLUME_MIXER_ALSA) == 0) {
			volume_mixerType = VOLUME_MIXER_TYPE_ALSA;
			volume_mixerDevice = VOLUME_MIXER_ALSA_DEFAULT;
		}
#endif
#ifdef HAVE_OSS
		else if (strcmp(param->value, VOLUME_MIXER_OSS) == 0) {
			volume_mixerType = VOLUME_MIXER_TYPE_OSS;
			volume_mixerDevice = VOLUME_MIXER_OSS_DEFAULT;
		}
#endif
		else if (strcmp(param->value, VOLUME_MIXER_SOFTWARE) == 0) {
			volume_mixerType = VOLUME_MIXER_TYPE_SOFTWARE;
			volume_mixerDevice = VOLUME_MIXER_SOFTWARE_DEFAULT;
		} else {
			FATAL("unknown mixer type %s at line %i\n",
			      param->value, param->line);
		}
	}

	param = getConfigParam(CONF_MIXER_DEVICE);

	if (param) {
		volume_mixerDevice = param->value;
	}
}

void openVolumeDevice(void)
{
	if (prepMixer(volume_mixerDevice) < 0) {
		WARNING("using software volume\n");
		volume_mixerType = VOLUME_MIXER_TYPE_SOFTWARE;
	}
}

static int getSoftwareVolume(void)
{
	return volume_softwareSet;
}

int getVolumeLevel(void)
{
	switch (volume_mixerType) {
#ifdef HAVE_ALSA
	case VOLUME_MIXER_TYPE_ALSA:
		return getAlsaVolumeLevel();
#endif
#ifdef HAVE_OSS
	case VOLUME_MIXER_TYPE_OSS:
		return getOssVolumeLevel();
#endif
	case VOLUME_MIXER_TYPE_SOFTWARE:
		return getSoftwareVolume();
	default:
		return -1;
	}
}

static int changeSoftwareVolume(int change, int rel)
{
	int new = change;

	if (rel)
		new += volume_softwareSet;

	if (new > 100)
		new = 100;
	else if (new < 0)
		new = 0;

	volume_softwareSet = new;

	/*new = 100.0*(exp(new/50.0)-1)/(M_E*M_E-1)+0.5; */
	if (new >= 100)
		new = PCM_VOLUME_1;
	else if (new <= 0)
		new = 0;
	else
		new = pcm_float_to_volume((exp(new / 25.0) - 1) /
					  (54.5981500331F - 1));

	setPlayerSoftwareVolume(new);

	return 0;
}

int changeVolumeLevel(int change, int rel)
{
	idle_add(IDLE_MIXER);

	switch (volume_mixerType) {
#ifdef HAVE_ALSA
	case VOLUME_MIXER_TYPE_ALSA:
		return changeAlsaVolumeLevel(change, rel);
#endif
#ifdef HAVE_OSS
	case VOLUME_MIXER_TYPE_OSS:
		return changeOssVolumeLevel(change, rel);
#endif
	case VOLUME_MIXER_TYPE_SOFTWARE:
		return changeSoftwareVolume(change, rel);
	default:
		return 0;
	}
}

void read_sw_volume_state(FILE *fp)
{
	char buf[sizeof(SW_VOLUME_STATE) + sizeof("100") - 1];
	char *end = NULL;
	long int sv;

	if (volume_mixerType != VOLUME_MIXER_TYPE_SOFTWARE)
		return;
	while (myFgets(buf, sizeof(buf), fp)) {
		if (!g_str_has_prefix(buf, SW_VOLUME_STATE))
			continue;
		sv = strtol(buf + strlen(SW_VOLUME_STATE), &end, 10);
		if (mpd_likely(!*end))
			changeSoftwareVolume(sv, 0);
		else
			ERROR("Can't parse software volume: %s\n", buf);
		return;
	}
}

void save_sw_volume_state(FILE *fp)
{
	if (volume_mixerType == VOLUME_MIXER_TYPE_SOFTWARE)
		fprintf(fp, SW_VOLUME_STATE "%d\n", volume_softwareSet);
}

