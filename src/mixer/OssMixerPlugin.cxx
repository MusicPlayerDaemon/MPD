/*
 * Copyright (C) 2003-2013 The Music Player Daemon Project
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
#include "MixerInternal.hxx"
#include "OutputAPI.hxx"
#include "system/fd_util.h"

#include <glib.h>

#include <assert.h>
#include <string.h>
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

class OssMixer : public Mixer {
	const char *device;
	const char *control;

	int device_fd;
	int volume_control;

public:
	OssMixer():Mixer(oss_mixer_plugin) {}

	bool Configure(const config_param &param, GError **error_r);
	bool Open(GError **error_r);
	void Close();

	int GetVolume(GError **error_r);
	bool SetVolume(unsigned volume, GError **error_r);
};

/**
 * The quark used for GError.domain.
 */
static inline GQuark
oss_mixer_quark(void)
{
	return g_quark_from_static_string("oss_mixer");
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

inline bool
OssMixer::Configure(const config_param &param, GError **error_r)
{
	device = param.GetBlockValue("mixer_device",
					     VOLUME_MIXER_OSS_DEFAULT);
	control = param.GetBlockValue("mixer_control");

	if (control != NULL) {
		volume_control = oss_find_mixer(control);
		if (volume_control < 0) {
			g_set_error(error_r, oss_mixer_quark(), 0,
				    "no such mixer control: %s", control);
			return false;
		}
	} else
		volume_control = SOUND_MIXER_PCM;

	return true;
}

static Mixer *
oss_mixer_init(gcc_unused void *ao, const config_param &param,
	       GError **error_r)
{
	OssMixer *om = new OssMixer();

	if (!om->Configure(param, error_r)) {
		delete om;
		return nullptr;
	}

	return om;
}

static void
oss_mixer_finish(Mixer *data)
{
	OssMixer *om = (OssMixer *) data;

	delete om;
}

void
OssMixer::Close()
{
	assert(device_fd >= 0);

	close(device_fd);
}

static void
oss_mixer_close(Mixer *data)
{
	OssMixer *om = (OssMixer *) data;
	om->Close();
}

inline bool
OssMixer::Open(GError **error_r)
{
	device_fd = open_cloexec(device, O_RDONLY, 0);
	if (device_fd < 0) {
		g_set_error(error_r, oss_mixer_quark(), errno,
			    "failed to open %s: %s",
			    device, g_strerror(errno));
		return false;
	}

	if (control) {
		int devmask = 0;

		if (ioctl(device_fd, SOUND_MIXER_READ_DEVMASK, &devmask) < 0) {
			g_set_error(error_r, oss_mixer_quark(), errno,
				    "READ_DEVMASK failed: %s",
				    g_strerror(errno));
			Close();
			return false;
		}

		if (((1 << volume_control) & devmask) == 0) {
			g_set_error(error_r, oss_mixer_quark(), 0,
				    "mixer control \"%s\" not usable",
				    control);
			Close();
			return false;
		}
	}

	return true;
}

static bool
oss_mixer_open(Mixer *data, GError **error_r)
{
	OssMixer *om = (OssMixer *) data;

	return om->Open(error_r);
}

inline int
OssMixer::GetVolume(GError **error_r)
{
	int left, right, level;
	int ret;

	assert(device_fd >= 0);

	ret = ioctl(device_fd, MIXER_READ(volume_control), &level);
	if (ret < 0) {
		g_set_error(error_r, oss_mixer_quark(), errno,
			    "failed to read OSS volume: %s",
			    g_strerror(errno));
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

static int
oss_mixer_get_volume(Mixer *mixer, GError **error_r)
{
	OssMixer *om = (OssMixer *)mixer;
	return om->GetVolume(error_r);
}

inline bool
OssMixer::SetVolume(unsigned volume, GError **error_r)
{
	int level;
	int ret;

	assert(device_fd >= 0);
	assert(volume <= 100);

	level = (volume << 8) + volume;

	ret = ioctl(device_fd, MIXER_WRITE(volume_control), &level);
	if (ret < 0) {
		g_set_error(error_r, oss_mixer_quark(), errno,
			    "failed to set OSS volume: %s",
			    g_strerror(errno));
		return false;
	}

	return true;
}

static bool
oss_mixer_set_volume(Mixer *mixer, unsigned volume, GError **error_r)
{
	OssMixer *om = (OssMixer *)mixer;
	return om->SetVolume(volume, error_r);
}

const struct mixer_plugin oss_mixer_plugin = {
	oss_mixer_init,
	oss_mixer_finish,
	oss_mixer_open,
	oss_mixer_close,
	oss_mixer_get_volume,
	oss_mixer_set_volume,
	true,
};
