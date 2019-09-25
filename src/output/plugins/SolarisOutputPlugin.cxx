/*
 * Copyright 2003-2019 The Music Player Daemon Project
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

#include "SolarisOutputPlugin.hxx"
#include "../OutputAPI.hxx"
#include "system/FileDescriptor.hxx"
#include "system/Error.hxx"

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#ifdef __sun
#include <sys/audio.h>
#include <sys/stropts.h>
#else

/* some fake declarations that allow build this plugin on systems
   other than Solaris, just to see if it compiles */

#include <sys/ioctl.h>

#ifndef I_FLUSH
#define I_FLUSH 0
#endif

#define AUDIO_GETINFO 0
#define AUDIO_SETINFO 0
#define AUDIO_ENCODING_LINEAR 0

struct audio_info {
	struct {
		unsigned sample_rate, channels, precision, encoding;
	} play;
};

#endif

class SolarisOutput final : AudioOutput {
	/* configuration */
	const char *const device;

	FileDescriptor fd;

	explicit SolarisOutput(const ConfigBlock &block)
		:AudioOutput(0),
		 device(block.GetBlockValue("device", "/dev/audio")) {}

public:
	static AudioOutput *Create(EventLoop &, const ConfigBlock &block) {
		return new SolarisOutput(block);
	}

private:
	void Open(AudioFormat &audio_format) override;
	void Close() noexcept override;

	size_t Play(const void *chunk, size_t size) override;
	void Cancel() noexcept override;
};

static bool
solaris_output_test_default_device(void)
{
	struct stat st;

	return stat("/dev/audio", &st) == 0 && S_ISCHR(st.st_mode) &&
		access("/dev/audio", W_OK) == 0;
}

void
SolarisOutput::Open(AudioFormat &audio_format)
{
	struct audio_info info;
	int ret;

	/* support only 16 bit mono/stereo for now; nothing else has
	   been tested */
	audio_format.format = SampleFormat::S16;

	/* open the device in non-blocking mode */

	if (!fd.Open(device, O_WRONLY|O_NONBLOCK))
		throw FormatErrno("Failed to open %s",
				  device);

	/* restore blocking mode */

	fd.SetBlocking();

	/* configure the audio device */

	ret = ioctl(fd.Get(), AUDIO_GETINFO, &info);
	if (ret < 0) {
		const int e = errno;
		fd.Close();
		throw MakeErrno(e, "AUDIO_GETINFO failed");
	}

	info.play.sample_rate = audio_format.sample_rate;
	info.play.channels = audio_format.channels;
	info.play.precision = 16;
	info.play.encoding = AUDIO_ENCODING_LINEAR;

	ret = ioctl(fd.Get(), AUDIO_SETINFO, &info);
	if (ret < 0) {
		const int e = errno;
		fd.Close();
		throw MakeErrno(e, "AUDIO_SETINFO failed");
	}
}

void
SolarisOutput::Close() noexcept
{
	fd.Close();
}

size_t
SolarisOutput::Play(const void *chunk, size_t size)
{
	ssize_t nbytes = fd.Write(chunk, size);
	if (nbytes <= 0)
		throw MakeErrno("Write failed");

	return nbytes;
}

void
SolarisOutput::Cancel() noexcept
{
	ioctl(fd.Get(), I_FLUSH);
}

const struct AudioOutputPlugin solaris_output_plugin = {
	"solaris",
	solaris_output_test_default_device,
	&SolarisOutput::Create,
	nullptr,
};
