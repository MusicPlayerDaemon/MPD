/*
 * Copyright (C) 2003-2014 The Music Player Daemon Project
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
#include "mixer/MixerInternal.hxx"
#include "config/ConfigData.hxx"
#include "system/fd_util.h"
#include "util/ASCII.hxx"
#include "util/Error.hxx"
#include "util/Domain.hxx"
#include "Log.hxx"

#include <assert.h>
#include <string.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>

#if defined(__OpenBSD__) || defined(__NetBSD__)
# include <soundcard.h>
#else /* !(defined(__OpenBSD__) || defined(__NetBSD__) */
# include <sys/soundcard.h>
#endif /* !(defined(__OpenBSD__) || defined(__NetBSD__) */

#define VOLUME_MIXER_OSS_DEFAULT		"/dev/mixer"

class OssMixer final : public Mixer {
	const char *device;
	const char *control;

	int device_fd;
	int volume_control;

public:
	OssMixer(MixerListener &_listener)
		:Mixer(oss_mixer_plugin, _listener) {}

	bool Configure(const config_param &param, Error &error);

	/* virtual methods from class Mixer */
	virtual bool Open(Error &error) override;
	virtual void Close() override;
	virtual int GetVolume(Error &error) override;
	virtual bool SetVolume(unsigned volume, Error &error) override;
};

static constexpr Domain oss_mixer_domain("oss_mixer");

static int
oss_find_mixer(const char *name)
{
	const char *labels[SOUND_MIXER_NRDEVICES] = SOUND_DEVICE_LABELS;
	size_t name_length = strlen(name);

	for (unsigned i = 0; i < SOUND_MIXER_NRDEVICES; i++) {
		if (StringEqualsCaseASCII(name, labels[i], name_length) &&
		    (labels[i][name_length] == 0 ||
		     labels[i][name_length] == ' '))
			return i;
	}
	return -1;
}

inline bool
OssMixer::Configure(const config_param &param, Error &error)
{
	device = param.GetBlockValue("mixer_device", VOLUME_MIXER_OSS_DEFAULT);
	control = param.GetBlockValue("mixer_control");

	if (control != NULL) {
		volume_control = oss_find_mixer(control);
		if (volume_control < 0) {
			error.Format(oss_mixer_domain, 0,
				     "no such mixer control: %s", control);
			return false;
		}
	} else
		volume_control = SOUND_MIXER_PCM;

	return true;
}

static Mixer *
oss_mixer_init(gcc_unused EventLoop &event_loop, gcc_unused AudioOutput &ao,
	       MixerListener &listener,
	       const config_param &param,
	       Error &error)
{
	OssMixer *om = new OssMixer(listener);

	if (!om->Configure(param, error)) {
		delete om;
		return nullptr;
	}

	return om;
}

void
OssMixer::Close()
{
	assert(device_fd >= 0);

	close(device_fd);
}

bool
OssMixer::Open(Error &error)
{
	device_fd = open_cloexec(device, O_RDONLY, 0);
	if (device_fd < 0) {
		error.FormatErrno("failed to open %s", device);
		return false;
	}

	if (control) {
		int devmask = 0;

		if (ioctl(device_fd, SOUND_MIXER_READ_DEVMASK, &devmask) < 0) {
			error.SetErrno("READ_DEVMASK failed");
			Close();
			return false;
		}

		if (((1 << volume_control) & devmask) == 0) {
			error.Format(oss_mixer_domain, 0,
				     "mixer control \"%s\" not usable",
				     control);
			Close();
			return false;
		}
	}

	return true;
}

int
OssMixer::GetVolume(Error &error)
{
	int left, right, level;
	int ret;

	assert(device_fd >= 0);

	ret = ioctl(device_fd, MIXER_READ(volume_control), &level);
	if (ret < 0) {
		error.SetErrno("failed to read OSS volume");
		return false;
	}

	left = level & 0xff;
	right = (level & 0xff00) >> 8;

	if (left != right) {
		FormatWarning(oss_mixer_domain,
			      "volume for left and right is not the same, \"%i\" and "
			      "\"%i\"\n", left, right);
	}

	return left;
}

bool
OssMixer::SetVolume(unsigned volume, Error &error)
{
	int level;
	int ret;

	assert(device_fd >= 0);
	assert(volume <= 100);

	level = (volume << 8) + volume;

	ret = ioctl(device_fd, MIXER_WRITE(volume_control), &level);
	if (ret < 0) {
		error.SetErrno("failed to set OSS volume");
		return false;
	}

	return true;
}

const MixerPlugin oss_mixer_plugin = {
	oss_mixer_init,
	true,
};
