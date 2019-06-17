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

#include "OssOutputPlugin.hxx"
#include "../OutputAPI.hxx"
#include "mixer/MixerList.hxx"
#include "system/UniqueFileDescriptor.hxx"
#include "system/Error.hxx"
#include "util/ConstBuffer.hxx"
#include "util/Domain.hxx"
#include "util/Macros.hxx"
#include "util/ByteOrder.hxx"
#include "Log.hxx"

#include <stdexcept>

#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>

#if defined(__OpenBSD__) || defined(__NetBSD__)
# include <soundcard.h>
#else /* !(defined(__OpenBSD__) || defined(__NetBSD__) */
# include <sys/soundcard.h>
#endif /* !(defined(__OpenBSD__) || defined(__NetBSD__) */

/* We got bug reports from FreeBSD users who said that the two 24 bit
   formats generate white noise on FreeBSD, but 32 bit works.  This is
   a workaround until we know what exactly is expected by the kernel
   audio drivers. */
#ifndef __linux__
#undef AFMT_S24_PACKED
#undef AFMT_S24_NE
#endif

#ifdef AFMT_S24_PACKED
#include "pcm/Export.hxx"
#include "util/Manual.hxx"
#endif

class OssOutput final : AudioOutput {
#ifdef AFMT_S24_PACKED
	Manual<PcmExport> pcm_export;
#endif

	FileDescriptor fd = FileDescriptor::Undefined();
	const char *device;

	/**
	 * The current input audio format.  This is needed to reopen
	 * the device after cancel().
	 */
	AudioFormat audio_format;

	/**
	 * The current OSS audio format.  This is needed to reopen the
	 * device after cancel().
	 */
	int oss_format;

#ifdef AFMT_S24_PACKED
	static constexpr unsigned oss_flags = FLAG_ENABLE_DISABLE;
#else
	static constexpr unsigned oss_flags = 0;
#endif

public:
	explicit OssOutput(const char *_device=nullptr)
		:AudioOutput(oss_flags),
		 device(_device) {}

	static AudioOutput *Create(EventLoop &event_loop,
				   const ConfigBlock &block);

#ifdef AFMT_S24_PACKED
	void Enable() override {
		pcm_export.Construct();
	}

	void Disable() noexcept override {
		pcm_export.Destruct();
	}
#endif

	void Open(AudioFormat &audio_format) override;

	void Close() noexcept override {
		DoClose();
	}

	size_t Play(const void *chunk, size_t size) override;
	void Cancel() noexcept override;

private:
	/**
	 * Sets up the OSS device which was opened before.
	 */
	void Setup(AudioFormat &audio_format);

	/**
	 * Reopen the device with the saved audio_format, without any probing.
	 *
	 * Throws #std::runtime_error on error.
	 */
	void Reopen();

	void DoClose();
};

static constexpr Domain oss_output_domain("oss_output");

enum oss_stat {
	OSS_STAT_NO_ERROR = 0,
	OSS_STAT_NOT_CHAR_DEV = -1,
	OSS_STAT_NO_PERMS = -2,
	OSS_STAT_DOESN_T_EXIST = -3,
	OSS_STAT_OTHER = -4,
};

static enum oss_stat
oss_stat_device(const char *device, int *errno_r)
{
	struct stat st;

	if (0 == stat(device, &st)) {
		if (!S_ISCHR(st.st_mode)) {
			return OSS_STAT_NOT_CHAR_DEV;
		}
	} else {
		*errno_r = errno;

		switch (errno) {
		case ENOENT:
		case ENOTDIR:
			return OSS_STAT_DOESN_T_EXIST;
		case EACCES:
			return OSS_STAT_NO_PERMS;
		default:
			return OSS_STAT_OTHER;
		}
	}

	return OSS_STAT_NO_ERROR;
}

static const char *const default_devices[] = { "/dev/sound/dsp", "/dev/dsp" };

static bool
oss_output_test_default_device(void)
{
	for (int i = ARRAY_SIZE(default_devices); --i >= 0; ) {
		UniqueFileDescriptor fd;
		if (fd.Open(default_devices[i], O_WRONLY, 0))
			return true;

		FormatErrno(oss_output_domain,
			    "Error opening OSS device \"%s\"",
			    default_devices[i]);
	}

	return false;
}

static OssOutput *
oss_open_default()
{
	int err[ARRAY_SIZE(default_devices)];
	enum oss_stat ret[ARRAY_SIZE(default_devices)];

	for (int i = ARRAY_SIZE(default_devices); --i >= 0; ) {
		ret[i] = oss_stat_device(default_devices[i], &err[i]);
		if (ret[i] == OSS_STAT_NO_ERROR)
			return new OssOutput(default_devices[i]);
	}

	for (int i = ARRAY_SIZE(default_devices); --i >= 0; ) {
		const char *dev = default_devices[i];
		switch(ret[i]) {
		case OSS_STAT_NO_ERROR:
			/* never reached */
			break;
		case OSS_STAT_DOESN_T_EXIST:
			FormatWarning(oss_output_domain,
				      "%s not found", dev);
			break;
		case OSS_STAT_NOT_CHAR_DEV:
			FormatWarning(oss_output_domain,
				      "%s is not a character device", dev);
			break;
		case OSS_STAT_NO_PERMS:
			FormatWarning(oss_output_domain,
				      "%s: permission denied", dev);
			break;
		case OSS_STAT_OTHER:
			FormatErrno(oss_output_domain, err[i],
				    "Error accessing %s", dev);
		}
	}

	throw std::runtime_error("error trying to open default OSS device");
}

AudioOutput *
OssOutput::Create(EventLoop &, const ConfigBlock &block)
{
	const char *device = block.GetBlockValue("device");
	if (device != nullptr)
		return new OssOutput(device);

	return oss_open_default();
}

void
OssOutput::DoClose()
{
	if (fd.IsDefined())
		fd.Close();
}

/**
 * A tri-state type for oss_try_ioctl().
 */
enum oss_setup_result {
	SUCCESS,
	UNSUPPORTED,
};

/**
 * Invoke an ioctl on the OSS file descriptor.  On success, SUCCESS is
 * returned.  If the parameter is not supported, UNSUPPORTED is
 * returned.  Any other failure throws std::runtime_error.
 */
static enum oss_setup_result
oss_try_ioctl_r(FileDescriptor fd, unsigned long request, int *value_r,
		const char *msg)
{
	assert(fd.IsDefined());
	assert(value_r != nullptr);
	assert(msg != nullptr);

	int ret = ioctl(fd.Get(), request, value_r);
	if (ret >= 0)
		return SUCCESS;

	if (errno == EINVAL)
		return UNSUPPORTED;

	throw MakeErrno(msg);
}

/**
 * Invoke an ioctl on the OSS file descriptor.  On success, SUCCESS is
 * returned.  If the parameter is not supported, UNSUPPORTED is
 * returned.  Any other failure throws std::runtime_error.
 */
static enum oss_setup_result
oss_try_ioctl(FileDescriptor fd, unsigned long request, int value,
	      const char *msg)
{
	return oss_try_ioctl_r(fd, request, &value, msg);
}

/**
 * Set up the channel number, and attempts to find alternatives if the
 * specified number is not supported.
 *
 * Throws #std::runtime_error on error.
 */
static void
oss_setup_channels(FileDescriptor fd, AudioFormat &audio_format)
{
	const char *const msg = "Failed to set channel count";
	int channels = audio_format.channels;
	enum oss_setup_result result =
		oss_try_ioctl_r(fd, SNDCTL_DSP_CHANNELS, &channels, msg);
	switch (result) {
	case SUCCESS:
		if (!audio_valid_channel_count(channels))
		    break;

		audio_format.channels = channels;
		return;

	case UNSUPPORTED:
		break;
	}

	for (unsigned i = 1; i < 2; ++i) {
		if (i == audio_format.channels)
			/* don't try that again */
			continue;

		channels = i;
		result = oss_try_ioctl_r(fd, SNDCTL_DSP_CHANNELS, &channels,
					 msg);
		switch (result) {
		case SUCCESS:
			if (!audio_valid_channel_count(channels))
			    break;

			audio_format.channels = channels;
			return;

		case UNSUPPORTED:
			break;
		}
	}

	throw std::runtime_error(msg);
}

/**
 * Set up the sample rate, and attempts to find alternatives if the
 * specified sample rate is not supported.
 *
 * Throws #std::runtime_error on error.
 */
static void
oss_setup_sample_rate(FileDescriptor fd, AudioFormat &audio_format)
{
	const char *const msg = "Failed to set sample rate";
	int sample_rate = audio_format.sample_rate;
	enum oss_setup_result result =
		oss_try_ioctl_r(fd, SNDCTL_DSP_SPEED, &sample_rate,
				msg);
	switch (result) {
	case SUCCESS:
		if (!audio_valid_sample_rate(sample_rate))
			break;

		audio_format.sample_rate = sample_rate;
		return;

	case UNSUPPORTED:
		break;
	}

	static constexpr int sample_rates[] = { 48000, 44100, 0 };
	for (unsigned i = 0; sample_rates[i] != 0; ++i) {
		sample_rate = sample_rates[i];
		if (sample_rate == (int)audio_format.sample_rate)
			continue;

		result = oss_try_ioctl_r(fd, SNDCTL_DSP_SPEED, &sample_rate,
					 msg);
		switch (result) {
		case SUCCESS:
			if (!audio_valid_sample_rate(sample_rate))
				break;

			audio_format.sample_rate = sample_rate;
			return;

		case UNSUPPORTED:
			break;
		}
	}

	throw std::runtime_error(msg);
}

/**
 * Convert a MPD sample format to its OSS counterpart.  Returns
 * AFMT_QUERY if there is no direct counterpart.
 */
gcc_const
static int
sample_format_to_oss(SampleFormat format) noexcept
{
	switch (format) {
	case SampleFormat::UNDEFINED:
	case SampleFormat::FLOAT:
	case SampleFormat::DSD:
		return AFMT_QUERY;

	case SampleFormat::S8:
		return AFMT_S8;

	case SampleFormat::S16:
		return AFMT_S16_NE;

	case SampleFormat::S24_P32:
#ifdef AFMT_S24_NE
		return AFMT_S24_NE;
#else
		return AFMT_QUERY;
#endif

	case SampleFormat::S32:
#ifdef AFMT_S32_NE
		return AFMT_S32_NE;
#else
		return AFMT_QUERY;
#endif
	}

	assert(false);
	gcc_unreachable();
}

/**
 * Convert an OSS sample format to its MPD counterpart.  Returns
 * SampleFormat::UNDEFINED if there is no direct counterpart.
 */
gcc_const
static SampleFormat
sample_format_from_oss(int format) noexcept
{
	switch (format) {
	case AFMT_S8:
		return SampleFormat::S8;

	case AFMT_S16_NE:
		return SampleFormat::S16;

#ifdef AFMT_S24_PACKED
	case AFMT_S24_PACKED:
		return SampleFormat::S24_P32;
#endif

#ifdef AFMT_S24_NE
	case AFMT_S24_NE:
		return SampleFormat::S24_P32;
#endif

#ifdef AFMT_S32_NE
	case AFMT_S32_NE:
		return SampleFormat::S32;
#endif

	default:
		return SampleFormat::UNDEFINED;
	}
}

/**
 * Probe one sample format.
 *
 * @return the selected sample format or SampleFormat::UNDEFINED on
 * error
 */
static enum oss_setup_result
oss_probe_sample_format(FileDescriptor fd, SampleFormat sample_format,
			SampleFormat *sample_format_r,
			int *oss_format_r
#ifdef AFMT_S24_PACKED
			, PcmExport &pcm_export
#endif
			)
{
	int oss_format = sample_format_to_oss(sample_format);
	if (oss_format == AFMT_QUERY)
		return UNSUPPORTED;

	enum oss_setup_result result =
		oss_try_ioctl_r(fd, SNDCTL_DSP_SAMPLESIZE,
				&oss_format,
				"Failed to set sample format");

#ifdef AFMT_S24_PACKED
	if (result == UNSUPPORTED && sample_format == SampleFormat::S24_P32) {
		/* if the driver doesn't support padded 24 bit, try
		   packed 24 bit */
		oss_format = AFMT_S24_PACKED;
		result = oss_try_ioctl_r(fd, SNDCTL_DSP_SAMPLESIZE,
					 &oss_format,
					 "Failed to set sample format");
	}
#endif

	if (result != SUCCESS)
		return result;

	sample_format = sample_format_from_oss(oss_format);
	if (sample_format == SampleFormat::UNDEFINED)
		return UNSUPPORTED;

	*sample_format_r = sample_format;
	*oss_format_r = oss_format;

#ifdef AFMT_S24_PACKED
	PcmExport::Params params;
	params.alsa_channel_order = true;
	params.pack24 = oss_format == AFMT_S24_PACKED;
	params.reverse_endian = oss_format == AFMT_S24_PACKED &&
		!IsLittleEndian();

	pcm_export.Open(sample_format, 0, params);
#endif

	return SUCCESS;
}

/**
 * Set up the sample format, and attempts to find alternatives if the
 * specified format is not supported.
 */
static void
oss_setup_sample_format(FileDescriptor fd, AudioFormat &audio_format,
			int *oss_format_r
#ifdef AFMT_S24_PACKED
			, PcmExport &pcm_export
#endif
			)
{
	SampleFormat mpd_format;
	enum oss_setup_result result =
		oss_probe_sample_format(fd, audio_format.format,
					&mpd_format, oss_format_r
#ifdef AFMT_S24_PACKED
					, pcm_export
#endif
					);
	switch (result) {
	case SUCCESS:
		audio_format.format = mpd_format;
		return;

	case UNSUPPORTED:
		break;
	}

	/* the requested sample format is not available - probe for
	   other formats supported by MPD */

	static constexpr SampleFormat sample_formats[] = {
		SampleFormat::S24_P32,
		SampleFormat::S32,
		SampleFormat::S16,
		SampleFormat::S8,
		SampleFormat::UNDEFINED /* sentinel */
	};

	for (unsigned i = 0; sample_formats[i] != SampleFormat::UNDEFINED; ++i) {
		mpd_format = sample_formats[i];
		if (mpd_format == audio_format.format)
			/* don't try that again */
			continue;

		result = oss_probe_sample_format(fd, mpd_format,
						 &mpd_format, oss_format_r
#ifdef AFMT_S24_PACKED
						 , pcm_export
#endif
						 );
		switch (result) {
		case SUCCESS:
			audio_format.format = mpd_format;
			return;

		case UNSUPPORTED:
			break;
		}
	}

	throw std::runtime_error("Failed to set sample format");
}

inline void
OssOutput::Setup(AudioFormat &_audio_format)
{
	oss_setup_channels(fd, _audio_format);
	oss_setup_sample_rate(fd, _audio_format);
	oss_setup_sample_format(fd, _audio_format, &oss_format
#ifdef AFMT_S24_PACKED
				, pcm_export
#endif
				);
}

/**
 * Reopen the device with the saved audio_format, without any probing.
 */
inline void
OssOutput::Reopen()
try {
	assert(!fd.IsDefined());

	if (!fd.Open(device, O_WRONLY))
		throw FormatErrno("Error opening OSS device \"%s\"", device);

	enum oss_setup_result result;

	const char *const msg1 = "Failed to set channel count";
	result = oss_try_ioctl(fd, SNDCTL_DSP_CHANNELS,
			       audio_format.channels, msg1);
	if (result != SUCCESS) {
		DoClose();
		throw std::runtime_error(msg1);
	}

	const char *const msg2 = "Failed to set sample rate";
	result = oss_try_ioctl(fd, SNDCTL_DSP_SPEED,
			       audio_format.sample_rate, msg2);
	if (result != SUCCESS) {
		DoClose();
		throw std::runtime_error(msg2);
	}

	const char *const msg3 = "Failed to set sample format";
	result = oss_try_ioctl(fd, SNDCTL_DSP_SAMPLESIZE,
			       oss_format,
			       msg3);
	if (result != SUCCESS) {
		DoClose();
		throw std::runtime_error(msg3);
	}
} catch (...) {
	DoClose();
	throw;
}

void
OssOutput::Open(AudioFormat &_audio_format)
try {
	if (!fd.Open(device, O_WRONLY))
		throw FormatErrno("Error opening OSS device \"%s\"", device);

	Setup(_audio_format);

	audio_format = _audio_format;
} catch (...) {
	DoClose();
	throw;
}

void
OssOutput::Cancel() noexcept
{
	if (fd.IsDefined()) {
		ioctl(fd.Get(), SNDCTL_DSP_RESET, 0);
		DoClose();
	}

#ifdef AFMT_S24_PACKED
	pcm_export->Reset();
#endif
}

size_t
OssOutput::Play(const void *chunk, size_t size)
{
	ssize_t ret;

	assert(size > 0);

	/* reopen the device since it was closed by dropBufferedAudio */
	if (!fd.IsDefined())
		Reopen();

#ifdef AFMT_S24_PACKED
	const auto e = pcm_export->Export({chunk, size});
	if (e.empty())
		return size;

	chunk = e.data;
	size = e.size;
#endif

	while (true) {
		ret = fd.Write(chunk, size);
		if (ret > 0) {
#ifdef AFMT_S24_PACKED
			ret = pcm_export->CalcSourceSize(ret);
#endif
			return ret;
		}

		if (ret < 0 && errno != EINTR)
			throw FormatErrno("Write error on %s", device);
	}
}

const struct AudioOutputPlugin oss_output_plugin = {
	"oss",
	oss_output_test_default_device,
	OssOutput::Create,
	&oss_mixer_plugin,
};
