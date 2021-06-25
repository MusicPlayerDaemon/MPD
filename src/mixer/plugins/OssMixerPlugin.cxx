/*
 * Copyright 2003-2021 The Music Player Daemon Project
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

#include "mixer/MixerInternal.hxx"
#include "config/Block.hxx"
#include "io/FileDescriptor.hxx"
#include "system/Error.hxx"
#include "util/ASCII.hxx"
#include "util/Domain.hxx"
#include "util/RuntimeError.hxx"
#include "Log.hxx"

#include <cassert>

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

	FileDescriptor device_fd;
	int volume_control;

public:
	OssMixer(MixerListener &_listener, const ConfigBlock &block)
		:Mixer(oss_mixer_plugin, _listener) {
		Configure(block);
	}

	void Configure(const ConfigBlock &block);

	/* virtual methods from class Mixer */
	void Open() override;
	void Close() noexcept override;
	int GetVolume() override;
	void SetVolume(unsigned volume) override;
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

inline void
OssMixer::Configure(const ConfigBlock &block)
{
	device = block.GetBlockValue("mixer_device", VOLUME_MIXER_OSS_DEFAULT);
	control = block.GetBlockValue("mixer_control");

	if (control != NULL) {
		volume_control = oss_find_mixer(control);
		if (volume_control < 0)
			throw FormatRuntimeError("no such mixer control: %s",
						 control);
	} else
		volume_control = SOUND_MIXER_PCM;
}

static Mixer *
oss_mixer_init([[maybe_unused]] EventLoop &event_loop,
	       [[maybe_unused]] AudioOutput &ao,
	       MixerListener &listener,
	       const ConfigBlock &block)
{
	return new OssMixer(listener, block);
}

void
OssMixer::Close() noexcept
{
	assert(device_fd.IsDefined());

	device_fd.Close();
}

void
OssMixer::Open()
{
	device_fd.OpenReadOnly(device);
	if (!device_fd.IsDefined())
		throw FormatErrno("failed to open %s", device);

	try {
		if (control) {
			int devmask = 0;

			if (ioctl(device_fd.Get(), SOUND_MIXER_READ_DEVMASK, &devmask) < 0)
				throw MakeErrno("READ_DEVMASK failed");

			if (((1 << volume_control) & devmask) == 0)
				throw FormatErrno("mixer control \"%s\" not usable",
						  control);
		}
	} catch (...) {
		Close();
		throw;
	}
}

int
OssMixer::GetVolume()
{
	int left, right, level;
	int ret;

	assert(device_fd.IsDefined());

	ret = ioctl(device_fd.Get(), MIXER_READ(volume_control), &level);
	if (ret < 0)
		throw MakeErrno("failed to read OSS volume");

	left = level & 0xff;
	right = (level & 0xff00) >> 8;

	if (left != right) {
		FmtWarning(oss_mixer_domain,
			   "volume for left and right is not the same, \"{}\" and "
			   "\"{}\"\n", left, right);
	}

	return left;
}

void
OssMixer::SetVolume(unsigned volume)
{
	int level;

	assert(device_fd.IsDefined());
	assert(volume <= 100);

	level = (volume << 8) + volume;

	if (ioctl(device_fd.Get(), MIXER_WRITE(volume_control), &level) < 0)
		throw MakeErrno("failed to set OSS volume");
}

constexpr MixerPlugin oss_mixer_plugin = {
	oss_mixer_init,
	true,
};
