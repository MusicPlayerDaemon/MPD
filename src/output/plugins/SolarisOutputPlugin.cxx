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
#include "SolarisOutputPlugin.hxx"
#include "../OutputAPI.hxx"
#include "system/fd_util.h"
#include "util/Error.hxx"

#include <sys/stropts.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#ifdef __sun
#include <sys/audio.h>
#else

/* some fake declarations that allow build this plugin on systems
   other than Solaris, just to see if it compiles */

#define AUDIO_GETINFO 0
#define AUDIO_SETINFO 0
#define AUDIO_ENCODING_LINEAR 0

struct audio_info {
	struct {
		unsigned sample_rate, channels, precision, encoding;
	} play;
};

#endif

struct SolarisOutput {
	AudioOutput base;

	/* configuration */
	const char *device;

	int fd;

	SolarisOutput()
		:base(solaris_output_plugin) {}

	bool Initialize(const config_param &param, Error &error_r) {
		return base.Configure(param, error_r);
	}
};

static bool
solaris_output_test_default_device(void)
{
	struct stat st;

	return stat("/dev/audio", &st) == 0 && S_ISCHR(st.st_mode) &&
		access("/dev/audio", W_OK) == 0;
}

static AudioOutput *
solaris_output_init(const config_param &param, Error &error_r)
{
	SolarisOutput *so = new SolarisOutput();
	if (!so->Initialize(param, error_r)) {
		delete so;
		return nullptr;
	}

	so->device = param.GetBlockValue("device", "/dev/audio");

	return &so->base;
}

static void
solaris_output_finish(AudioOutput *ao)
{
	SolarisOutput *so = (SolarisOutput *)ao;

	delete so;
}

static bool
solaris_output_open(AudioOutput *ao, AudioFormat &audio_format,
		    Error &error)
{
	SolarisOutput *so = (SolarisOutput *)ao;
	struct audio_info info;
	int ret, flags;

	/* support only 16 bit mono/stereo for now; nothing else has
	   been tested */
	audio_format.format = SampleFormat::S16;

	/* open the device in non-blocking mode */

	so->fd = open_cloexec(so->device, O_WRONLY|O_NONBLOCK, 0);
	if (so->fd < 0) {
		error.FormatErrno("Failed to open %s",
				  so->device);
		return false;
	}

	/* restore blocking mode */

	flags = fcntl(so->fd, F_GETFL);
	if (flags > 0 && (flags & O_NONBLOCK) != 0)
		fcntl(so->fd, F_SETFL, flags & ~O_NONBLOCK);

	/* configure the audio device */

	ret = ioctl(so->fd, AUDIO_GETINFO, &info);
	if (ret < 0) {
		error.SetErrno("AUDIO_GETINFO failed");
		close(so->fd);
		return false;
	}

	info.play.sample_rate = audio_format.sample_rate;
	info.play.channels = audio_format.channels;
	info.play.precision = 16;
	info.play.encoding = AUDIO_ENCODING_LINEAR;

	ret = ioctl(so->fd, AUDIO_SETINFO, &info);
	if (ret < 0) {
		error.SetErrno("AUDIO_SETINFO failed");
		close(so->fd);
		return false;
	}

	return true;
}

static void
solaris_output_close(AudioOutput *ao)
{
	SolarisOutput *so = (SolarisOutput *)ao;

	close(so->fd);
}

static size_t
solaris_output_play(AudioOutput *ao, const void *chunk, size_t size,
		    Error &error)
{
	SolarisOutput *so = (SolarisOutput *)ao;
	ssize_t nbytes;

	nbytes = write(so->fd, chunk, size);
	if (nbytes <= 0) {
		error.SetErrno("Write failed");
		return 0;
	}

	return nbytes;
}

static void
solaris_output_cancel(AudioOutput *ao)
{
	SolarisOutput *so = (SolarisOutput *)ao;

	ioctl(so->fd, I_FLUSH);
}

const struct AudioOutputPlugin solaris_output_plugin = {
	"solaris",
	solaris_output_test_default_device,
	solaris_output_init,
	solaris_output_finish,
	nullptr,
	nullptr,
	solaris_output_open,
	solaris_output_close,
	nullptr,
	nullptr,
	solaris_output_play,
	nullptr,
	solaris_output_cancel,
	nullptr,
	nullptr,
};
