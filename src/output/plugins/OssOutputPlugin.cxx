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
#include "OssOutputPlugin.hxx"
#include "../OutputAPI.hxx"
#include "mixer/MixerList.hxx"
#include "system/fd_util.h"
#include "util/ConstBuffer.hxx"
#include "util/Error.hxx"
#include "util/Domain.hxx"
#include "util/Macros.hxx"
#include "system/ByteOrder.hxx"
#include "Log.hxx"

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
#include "pcm/PcmExport.hxx"
#include "util/Manual.hxx"
#endif

struct OssOutput {
	AudioOutput base;

#ifdef AFMT_S24_PACKED
	Manual<PcmExport> pcm_export;
#endif

	int fd;
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

	OssOutput()
		:base(oss_output_plugin),
		 fd(-1), device(nullptr) {}

	bool Initialize(const config_param &param, Error &error_r) {
		return base.Configure(param, error_r);
	}
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

static const char *default_devices[] = { "/dev/sound/dsp", "/dev/dsp" };

static bool
oss_output_test_default_device(void)
{
	int fd, i;

	for (i = ARRAY_SIZE(default_devices); --i >= 0; ) {
		fd = open_cloexec(default_devices[i], O_WRONLY, 0);

		if (fd >= 0) {
			close(fd);
			return true;
		}

		FormatErrno(oss_output_domain,
			    "Error opening OSS device \"%s\"",
			    default_devices[i]);
	}

	return false;
}

static AudioOutput *
oss_open_default(Error &error)
{
	int err[ARRAY_SIZE(default_devices)];
	enum oss_stat ret[ARRAY_SIZE(default_devices)];

	const config_param empty;
	for (int i = ARRAY_SIZE(default_devices); --i >= 0; ) {
		ret[i] = oss_stat_device(default_devices[i], &err[i]);
		if (ret[i] == OSS_STAT_NO_ERROR) {
			OssOutput *od = new OssOutput();
			if (!od->Initialize(empty, error)) {
				delete od;
				return NULL;
			}

			od->device = default_devices[i];
			return &od->base;
		}
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

	error.Set(oss_output_domain,
		  "error trying to open default OSS device");
	return NULL;
}

static AudioOutput *
oss_output_init(const config_param &param, Error &error)
{
	const char *device = param.GetBlockValue("device");
	if (device != NULL) {
		OssOutput *od = new OssOutput();
		if (!od->Initialize(param, error)) {
			delete od;
			return NULL;
		}

		od->device = device;
		return &od->base;
	}

	return oss_open_default(error);
}

static void
oss_output_finish(AudioOutput *ao)
{
	OssOutput *od = (OssOutput *)ao;

	delete od;
}

#ifdef AFMT_S24_PACKED

static bool
oss_output_enable(AudioOutput *ao, gcc_unused Error &error)
{
	OssOutput *od = (OssOutput *)ao;

	od->pcm_export.Construct();
	return true;
}

static void
oss_output_disable(AudioOutput *ao)
{
	OssOutput *od = (OssOutput *)ao;

	od->pcm_export.Destruct();
}

#endif

static void
oss_close(OssOutput *od)
{
	if (od->fd >= 0)
		close(od->fd);
	od->fd = -1;
}

/**
 * A tri-state type for oss_try_ioctl().
 */
enum oss_setup_result {
	SUCCESS,
	ERROR,
	UNSUPPORTED,
};

/**
 * Invoke an ioctl on the OSS file descriptor.  On success, SUCCESS is
 * returned.  If the parameter is not supported, UNSUPPORTED is
 * returned.  Any other failure returns ERROR and allocates an #Error.
 */
static enum oss_setup_result
oss_try_ioctl_r(int fd, unsigned long request, int *value_r,
		const char *msg, Error &error)
{
	assert(fd >= 0);
	assert(value_r != NULL);
	assert(msg != NULL);
	assert(!error.IsDefined());

	int ret = ioctl(fd, request, value_r);
	if (ret >= 0)
		return SUCCESS;

	if (errno == EINVAL)
		return UNSUPPORTED;

	error.SetErrno(msg);
	return ERROR;
}

/**
 * Invoke an ioctl on the OSS file descriptor.  On success, SUCCESS is
 * returned.  If the parameter is not supported, UNSUPPORTED is
 * returned.  Any other failure returns ERROR and allocates an #Error.
 */
static enum oss_setup_result
oss_try_ioctl(int fd, unsigned long request, int value,
	      const char *msg, Error &error_r)
{
	return oss_try_ioctl_r(fd, request, &value, msg, error_r);
}

/**
 * Set up the channel number, and attempts to find alternatives if the
 * specified number is not supported.
 */
static bool
oss_setup_channels(int fd, AudioFormat &audio_format, Error &error)
{
	const char *const msg = "Failed to set channel count";
	int channels = audio_format.channels;
	enum oss_setup_result result =
		oss_try_ioctl_r(fd, SNDCTL_DSP_CHANNELS, &channels, msg, error);
	switch (result) {
	case SUCCESS:
		if (!audio_valid_channel_count(channels))
		    break;

		audio_format.channels = channels;
		return true;

	case ERROR:
		return false;

	case UNSUPPORTED:
		break;
	}

	for (unsigned i = 1; i < 2; ++i) {
		if (i == audio_format.channels)
			/* don't try that again */
			continue;

		channels = i;
		result = oss_try_ioctl_r(fd, SNDCTL_DSP_CHANNELS, &channels,
					 msg, error);
		switch (result) {
		case SUCCESS:
			if (!audio_valid_channel_count(channels))
			    break;

			audio_format.channels = channels;
			return true;

		case ERROR:
			return false;

		case UNSUPPORTED:
			break;
		}
	}

	error.Set(oss_output_domain, msg);
	return false;
}

/**
 * Set up the sample rate, and attempts to find alternatives if the
 * specified sample rate is not supported.
 */
static bool
oss_setup_sample_rate(int fd, AudioFormat &audio_format,
		      Error &error)
{
	const char *const msg = "Failed to set sample rate";
	int sample_rate = audio_format.sample_rate;
	enum oss_setup_result result =
		oss_try_ioctl_r(fd, SNDCTL_DSP_SPEED, &sample_rate,
				msg, error);
	switch (result) {
	case SUCCESS:
		if (!audio_valid_sample_rate(sample_rate))
			break;

		audio_format.sample_rate = sample_rate;
		return true;

	case ERROR:
		return false;

	case UNSUPPORTED:
		break;
	}

	static const int sample_rates[] = { 48000, 44100, 0 };
	for (unsigned i = 0; sample_rates[i] != 0; ++i) {
		sample_rate = sample_rates[i];
		if (sample_rate == (int)audio_format.sample_rate)
			continue;

		result = oss_try_ioctl_r(fd, SNDCTL_DSP_SPEED, &sample_rate,
					 msg, error);
		switch (result) {
		case SUCCESS:
			if (!audio_valid_sample_rate(sample_rate))
				break;

			audio_format.sample_rate = sample_rate;
			return true;

		case ERROR:
			return false;

		case UNSUPPORTED:
			break;
		}
	}

	error.Set(oss_output_domain, msg);
	return false;
}

/**
 * Convert a MPD sample format to its OSS counterpart.  Returns
 * AFMT_QUERY if there is no direct counterpart.
 */
static int
sample_format_to_oss(SampleFormat format)
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

	return AFMT_QUERY;
}

/**
 * Convert an OSS sample format to its MPD counterpart.  Returns
 * SampleFormat::UNDEFINED if there is no direct counterpart.
 */
static SampleFormat
sample_format_from_oss(int format)
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
oss_probe_sample_format(int fd, SampleFormat sample_format,
			SampleFormat *sample_format_r,
			int *oss_format_r,
#ifdef AFMT_S24_PACKED
			PcmExport &pcm_export,
#endif
			Error &error)
{
	int oss_format = sample_format_to_oss(sample_format);
	if (oss_format == AFMT_QUERY)
		return UNSUPPORTED;

	enum oss_setup_result result =
		oss_try_ioctl_r(fd, SNDCTL_DSP_SAMPLESIZE,
				&oss_format,
				"Failed to set sample format", error);

#ifdef AFMT_S24_PACKED
	if (result == UNSUPPORTED && sample_format == SampleFormat::S24_P32) {
		/* if the driver doesn't support padded 24 bit, try
		   packed 24 bit */
		oss_format = AFMT_S24_PACKED;
		result = oss_try_ioctl_r(fd, SNDCTL_DSP_SAMPLESIZE,
					 &oss_format,
					 "Failed to set sample format", error);
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
	pcm_export.Open(sample_format, 0, false, false,
			oss_format == AFMT_S24_PACKED,
			oss_format == AFMT_S24_PACKED &&
			!IsLittleEndian());
#endif

	return SUCCESS;
}

/**
 * Set up the sample format, and attempts to find alternatives if the
 * specified format is not supported.
 */
static bool
oss_setup_sample_format(int fd, AudioFormat &audio_format,
			int *oss_format_r,
#ifdef AFMT_S24_PACKED
			PcmExport &pcm_export,
#endif
			Error &error)
{
	SampleFormat mpd_format;
	enum oss_setup_result result =
		oss_probe_sample_format(fd, audio_format.format,
					&mpd_format, oss_format_r,
#ifdef AFMT_S24_PACKED
					pcm_export,
#endif
					error);
	switch (result) {
	case SUCCESS:
		audio_format.format = mpd_format;
		return true;

	case ERROR:
		return false;

	case UNSUPPORTED:
		break;
	}

	if (result != UNSUPPORTED)
		return result == SUCCESS;

	/* the requested sample format is not available - probe for
	   other formats supported by MPD */

	static const SampleFormat sample_formats[] = {
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
						 &mpd_format, oss_format_r,
#ifdef AFMT_S24_PACKED
						 pcm_export,
#endif
						 error);
		switch (result) {
		case SUCCESS:
			audio_format.format = mpd_format;
			return true;

		case ERROR:
			return false;

		case UNSUPPORTED:
			break;
		}
	}

	error.Set(oss_output_domain, "Failed to set sample format");
	return false;
}

/**
 * Sets up the OSS device which was opened before.
 */
static bool
oss_setup(OssOutput *od, AudioFormat &audio_format,
	  Error &error)
{
	return oss_setup_channels(od->fd, audio_format, error) &&
		oss_setup_sample_rate(od->fd, audio_format, error) &&
		oss_setup_sample_format(od->fd, audio_format, &od->oss_format,
#ifdef AFMT_S24_PACKED
					od->pcm_export,
#endif
					error);
}

/**
 * Reopen the device with the saved audio_format, without any probing.
 */
static bool
oss_reopen(OssOutput *od, Error &error)
{
	assert(od->fd < 0);

	od->fd = open_cloexec(od->device, O_WRONLY, 0);
	if (od->fd < 0) {
		error.FormatErrno("Error opening OSS device \"%s\"",
				  od->device);
		return false;
	}

	enum oss_setup_result result;

	const char *const msg1 = "Failed to set channel count";
	result = oss_try_ioctl(od->fd, SNDCTL_DSP_CHANNELS,
			       od->audio_format.channels, msg1, error);
	if (result != SUCCESS) {
		oss_close(od);
		if (result == UNSUPPORTED)
			error.Set(oss_output_domain, msg1);
		return false;
	}

	const char *const msg2 = "Failed to set sample rate";
	result = oss_try_ioctl(od->fd, SNDCTL_DSP_SPEED,
			       od->audio_format.sample_rate, msg2, error);
	if (result != SUCCESS) {
		oss_close(od);
		if (result == UNSUPPORTED)
			error.Set(oss_output_domain, msg2);
		return false;
	}

	const char *const msg3 = "Failed to set sample format";
	result = oss_try_ioctl(od->fd, SNDCTL_DSP_SAMPLESIZE,
			       od->oss_format,
			       msg3, error);
	if (result != SUCCESS) {
		oss_close(od);
		if (result == UNSUPPORTED)
			error.Set(oss_output_domain, msg3);
		return false;
	}

	return true;
}

static bool
oss_output_open(AudioOutput *ao, AudioFormat &audio_format,
		Error &error)
{
	OssOutput *od = (OssOutput *)ao;

	od->fd = open_cloexec(od->device, O_WRONLY, 0);
	if (od->fd < 0) {
		error.FormatErrno("Error opening OSS device \"%s\"",
				  od->device);
		return false;
	}

	if (!oss_setup(od, audio_format, error)) {
		oss_close(od);
		return false;
	}

	od->audio_format = audio_format;
	return true;
}

static void
oss_output_close(AudioOutput *ao)
{
	OssOutput *od = (OssOutput *)ao;

	oss_close(od);
}

static void
oss_output_cancel(AudioOutput *ao)
{
	OssOutput *od = (OssOutput *)ao;

	if (od->fd >= 0) {
		ioctl(od->fd, SNDCTL_DSP_RESET, 0);
		oss_close(od);
	}
}

static size_t
oss_output_play(AudioOutput *ao, const void *chunk, size_t size,
		Error &error)
{
	OssOutput *od = (OssOutput *)ao;
	ssize_t ret;

	assert(size > 0);

	/* reopen the device since it was closed by dropBufferedAudio */
	if (od->fd < 0 && !oss_reopen(od, error))
		return 0;

#ifdef AFMT_S24_PACKED
	const auto e = od->pcm_export->Export({chunk, size});
	chunk = e.data;
	size = e.size;
#endif

	assert(size > 0);

	while (true) {
		ret = write(od->fd, chunk, size);
		if (ret > 0) {
#ifdef AFMT_S24_PACKED
			ret = od->pcm_export->CalcSourceSize(ret);
#endif
			return ret;
		}

		if (ret < 0 && errno != EINTR) {
			error.FormatErrno("Write error on %s", od->device);
			return 0;
		}
	}
}

const struct AudioOutputPlugin oss_output_plugin = {
	"oss",
	oss_output_test_default_device,
	oss_output_init,
	oss_output_finish,
#ifdef AFMT_S24_PACKED
	oss_output_enable,
	oss_output_disable,
#else
	nullptr,
	nullptr,
#endif
	oss_output_open,
	oss_output_close,
	nullptr,
	nullptr,
	oss_output_play,
	nullptr,
	oss_output_cancel,
	nullptr,

	&oss_mixer_plugin,
};
