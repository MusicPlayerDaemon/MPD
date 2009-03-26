/*
 * Copyright (C) 2003-2009 The Music Player Daemon Project
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

#include "../output_api.h"
#include "../mixer_api.h"

#include <glib.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>

#if defined(__OpenBSD__) || defined(__NetBSD__)
# include <soundcard.h>
#else /* !(defined(__OpenBSD__) || defined(__NetBSD__) */
# include <sys/soundcard.h>
#endif /* !(defined(__OpenBSD__) || defined(__NetBSD__) */

#define VOLUME_MIXER_OSS_DEFAULT		"/dev/mixer"

struct oss_mixer {
	/** the base mixer class */
	struct mixer base;

	const char *device;
	const char *control;

	int device_fd;
	int volume_control;
};

static struct mixer *
oss_mixer_init(const struct config_param *param)
{
	struct oss_mixer *om = g_new(struct oss_mixer, 1);

	mixer_init(&om->base, &oss_mixer);

	om->device = config_get_block_string(param, "mixer_device",
					     VOLUME_MIXER_OSS_DEFAULT);
	om->control = config_get_block_string(param, "mixer_control", NULL);

	om->device_fd = -1;
	om->volume_control = SOUND_MIXER_PCM;

	return &om->base;
}

static void
oss_mixer_finish(struct mixer *data)
{
	struct oss_mixer *om = (struct oss_mixer *) data;

	g_free(om);
}

static void
oss_mixer_close(struct mixer *data)
{
	struct oss_mixer *om = (struct oss_mixer *) data;
	if (om->device_fd != -1)
		while (close(om->device_fd) && errno == EINTR) ;
	om->device_fd = -1;
}

static int
oss_find_mixer(const char *name)
{
	const char *labels[SOUND_MIXER_NRDEVICES] = SOUND_DEVICE_LABELS;
	size_t name_length = strlen(name);

	for (unsigned i = 0; i < SOUND_MIXER_NRDEVICES; i++) {
		if (g_ascii_strncasecmp(name, labels[i], name_length) == 0 &&
		    (labels[i][name_length] == 0 ||
		     labels[i][name_length] == ' '))
			return i;
	}
	return -1;
}

static bool
oss_mixer_open(struct mixer *data)
{
	struct oss_mixer *om = (struct oss_mixer *) data;

	om->device_fd = open(om->device, O_RDONLY);
	if (om->device_fd < 0) {
		g_warning("Unable to open oss mixer \"%s\"\n", om->device);
		return false;
	}

	if (om->control) {
		int i;
		int devmask = 0;

		if (ioctl(om->device_fd, SOUND_MIXER_READ_DEVMASK, &devmask) < 0) {
			g_warning("errors getting read_devmask for oss mixer\n");
			oss_mixer_close(data);
			return false;
		}
		i = oss_find_mixer(om->control);

		if (i < 0) {
			g_warning("mixer control \"%s\" not found\n",
				om->control);
			oss_mixer_close(data);
			return false;
		} else if (!((1 << i) & devmask)) {
			g_warning("mixer control \"%s\" not usable\n",
				om->control);
			oss_mixer_close(data);
			return false;
		}
		om->volume_control = i;
	}
	return true;
}

static int
oss_mixer_get_volume(struct mixer *mixer)
{
	struct oss_mixer *om = (struct oss_mixer *)mixer;
	int left, right, level;
	int ret;

	if (om->device_fd < 0 && !oss_mixer_open(mixer))
		return false;

	ret = ioctl(om->device_fd, MIXER_READ(om->volume_control), &level);
	if (ret < 0) {
		g_warning("unable to read oss volume\n");
		return false;
	}

	left = level & 0xff;
	right = (level & 0xff00) >> 8;

	if (left != right) {
		g_warning("volume for left and right is not the same, \"%i\" and "
			  "\"%i\"\n", left, right);
	}

	return left;
}

static bool
oss_mixer_set_volume(struct mixer *mixer, unsigned volume)
{
	struct oss_mixer *om = (struct oss_mixer *)mixer;
	int level;
	int ret;

	if (om->device_fd < 0 && !oss_mixer_open(mixer))
		return false;

	if (volume > 100)
		volume = 100;

	level = (volume << 8) + volume;

	ret = ioctl(om->device_fd, MIXER_WRITE(om->volume_control), &level);
	if (ret < 0) {
		g_warning("unable to set oss volume\n");
		return false;
	}

	return true;
}

const struct mixer_plugin oss_mixer = {
	.init = oss_mixer_init,
	.finish = oss_mixer_finish,
	.open = oss_mixer_open,
	.close = oss_mixer_close,
	.get_volume = oss_mixer_get_volume,
	.set_volume = oss_mixer_set_volume,
	.global = true,
};
