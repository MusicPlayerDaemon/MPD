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

#include "OssOutputPlugin.hxx"
#include "../OutputAPI.hxx"
#include "mixer/MixerList.hxx"
#include "pcm/Export.hxx"
#include "io/UniqueFileDescriptor.hxx"
#include "system/Error.hxx"
#include "util/ConstBuffer.hxx"
#include "util/Domain.hxx"
#include "util/ByteOrder.hxx"
#include "util/Manual.hxx"
#include "Log.hxx"

#include <cassert>
#include <cerrno>
#include <iterator>
#include <stdexcept>

#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>

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

#if defined(ENABLE_DSD) && defined(AFMT_S32_NE)
#define ENABLE_OSS_DSD
#endif

class OssOutput final : AudioOutput {
	Manual<PcmExport> pcm_export;

#ifdef ENABLE_OSS_DSD
	/**
	 * Enable DSD over PCM according to the DoP standard?
	 *
	 * @see http://dsd-guide.com/dop-open-standard
	 *
	 * this is default in oss as no other dsd-method is known to man
	 */
	const bool dop_setting;
#endif

	FileDescriptor fd = FileDescriptor::Undefined();
	const char *device;

	/**
	 * The effective audio format settings of the OSS device.
	 * This is needed by Reopen() after Cancel().
	 */
	int effective_channels, effective_speed, effective_samplesize;

	static constexpr unsigned oss_flags = FLAG_ENABLE_DISABLE;

public:
	explicit OssOutput(const char *_device=nullptr
#ifdef ENABLE_OSS_DSD
			   , bool dop = false
#endif
			   )
		:AudioOutput(oss_flags),
#ifdef ENABLE_OSS_DSD
		 dop_setting(dop),
#endif
		 device(_device)
	{
	}

	static AudioOutput *Create(EventLoop &event_loop,
				   const ConfigBlock &block);

	void Enable() override {
		pcm_export.Construct();
	}

	void Disable() noexcept override {
		pcm_export.Destruct();
	}

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

#ifdef ENABLE_OSS_DSD
	void SetupDop(const AudioFormat &audio_format);
#endif

	void SetupOrDop(AudioFormat &audio_format);

	/**
	 * Reopen the device with the saved audio_format, without any probing.
	 *
	 * Throws on error.
	 */
	void Reopen();

	void DoClose() noexcept;
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
oss_stat_device(const char *device, int *errno_r) noexcept
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
oss_output_test_default_device() noexcept
{
	for (int i = std::size(default_devices); --i >= 0; ) {
		UniqueFileDescriptor fd;
		if (fd.Open(default_devices[i], O_WRONLY, 0))
			return true;

		FmtError(oss_output_domain,
			 "Error opening OSS device \"{}\": {}",
			 default_devices[i], strerror(errno));
	}

	return false;
}

static OssOutput *
oss_open_default(
#ifdef ENABLE_OSS_DSD
		 bool dop
#endif
		 )
{
	int err[std::size(default_devices)];
	enum oss_stat ret[std::size(default_devices)];

	for (int i = std::size(default_devices); --i >= 0; ) {
		ret[i] = oss_stat_device(default_devices[i], &err[i]);
		if (ret[i] == OSS_STAT_NO_ERROR)
			return new OssOutput(default_devices[i]
#ifdef ENABLE_OSS_DSD
					     , dop
#endif
					     );
	}

	for (int i = std::size(default_devices); --i >= 0; ) {
		const char *dev = default_devices[i];
		switch(ret[i]) {
		case OSS_STAT_NO_ERROR:
			/* never reached */
			break;
		case OSS_STAT_DOESN_T_EXIST:
			FmtWarning(oss_output_domain,
				   "{} not found", dev);
			break;
		case OSS_STAT_NOT_CHAR_DEV:
			FmtWarning(oss_output_domain,
				   "{} is not a character device", dev);
			break;
		case OSS_STAT_NO_PERMS:
			FmtWarning(oss_output_domain,
				   "{}: permission denied", dev);
			break;
		case OSS_STAT_OTHER:
			FmtError(oss_output_domain, "Error accessing {}: {}",
				 dev, strerror(err[i]));
		}
	}

	throw std::runtime_error("error trying to open default OSS device");
}

AudioOutput *
OssOutput::Create(EventLoop &, const ConfigBlock &block)
{
#ifdef ENABLE_OSS_DSD
	bool dop = block.GetBlockValue("dop", false);
#endif

	const char *device = block.GetBlockValue("device");
	if (device != nullptr)
		return new OssOutput(device
#ifdef ENABLE_OSS_DSD
				     , dop
#endif
				     );

	return oss_open_default(
#ifdef ENABLE_OSS_DSD
				dop
#endif
				);
}

void
OssOutput::DoClose() noexcept
{
	if (fd.IsDefined())
		fd.Close();
}

/**
 * Invoke an ioctl on the OSS file descriptor.
 *
 * Throws on error.
 *
 * @return true success, false if the parameter is not supported
 */
static bool
oss_try_ioctl_r(FileDescriptor fd, unsigned long request, int *value_r,
		const char *msg)
{
	assert(fd.IsDefined());
	assert(value_r != nullptr);
	assert(msg != nullptr);

	int ret = ioctl(fd.Get(), request, value_r);
	if (ret >= 0)
		return true;

	if (errno == EINVAL)
		return false;

	throw MakeErrno(msg);
}

/**
 * Invoke an ioctl on the OSS file descriptor, and expect an
 * unmodified effective value.
 *
 * Throws on error.
 */
static void
OssIoctlExact(FileDescriptor fd, unsigned long request, int requested_value,
	      const char *msg)
{
	assert(fd.IsDefined());
	assert(msg != nullptr);

	int effective_value = requested_value;
	if (ioctl(fd.Get(), request, &effective_value) < 0)
		throw MakeErrno(msg);

	if (effective_value != requested_value)
		throw std::runtime_error(msg);
}

/**
 * Set up the channel number, and attempts to find alternatives if the
 * specified number is not supported.
 *
 * Throws on error.
 */
static void
oss_setup_channels(FileDescriptor fd, AudioFormat &audio_format,
		   int &effective_channels)
{
	const char *const msg = "Failed to set channel count";

	effective_channels = audio_format.channels;

	if (oss_try_ioctl_r(fd, SNDCTL_DSP_CHANNELS,
			    &effective_channels, msg) &&
	    audio_valid_channel_count(effective_channels)) {
		audio_format.channels = effective_channels;
		return;
	}

	for (unsigned i = 1; i < 2; ++i) {
		if (i == audio_format.channels)
			/* don't try that again */
			continue;

		effective_channels = i;
		if (oss_try_ioctl_r(fd, SNDCTL_DSP_CHANNELS,
				    &effective_channels, msg) &&
		    audio_valid_channel_count(effective_channels)) {
			audio_format.channels = effective_channels;
			return;
		}
	}

	throw std::runtime_error(msg);
}

/**
 * Set up the sample rate, and attempts to find alternatives if the
 * specified sample rate is not supported.
 *
 * Throws on error.
 */
static void
oss_setup_sample_rate(FileDescriptor fd, AudioFormat &audio_format,
		      int &effective_speed)
{
	const char *const msg = "Failed to set sample rate";

	effective_speed = audio_format.sample_rate;
	if (oss_try_ioctl_r(fd, SNDCTL_DSP_SPEED, &effective_speed, msg) &&
	    audio_valid_sample_rate(effective_speed)) {
		audio_format.sample_rate = effective_speed;
		return;
	}

	static constexpr int sample_rates[] = { 48000, 44100, 0 };
	for (unsigned i = 0; sample_rates[i] != 0; ++i) {
		effective_speed = sample_rates[i];
		if (effective_speed == (int)audio_format.sample_rate)
			continue;

		if (oss_try_ioctl_r(fd, SNDCTL_DSP_SPEED, &effective_speed, msg) &&
		    audio_valid_sample_rate(effective_speed)) {
			audio_format.sample_rate = effective_speed;
			return;
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
 * Throws on error.
 *
 * @return true success, false if the parameter is not supported
 */
static bool
oss_probe_sample_format(FileDescriptor fd, SampleFormat sample_format,
			SampleFormat *sample_format_r,
			int *oss_format_r,
			PcmExport &pcm_export)
{
	int oss_format = sample_format_to_oss(sample_format);
	if (oss_format == AFMT_QUERY)
		return false;

	bool success =
		oss_try_ioctl_r(fd, SNDCTL_DSP_SAMPLESIZE,
				&oss_format,
				"Failed to set sample format");

#ifdef AFMT_S24_PACKED
	if (!success && sample_format == SampleFormat::S24_P32) {
		/* if the driver doesn't support padded 24 bit, try
		   packed 24 bit */
		oss_format = AFMT_S24_PACKED;
		success = oss_try_ioctl_r(fd, SNDCTL_DSP_SAMPLESIZE,
					  &oss_format,
					  "Failed to set sample format");
	}
#endif

	if (!success)
		return false;

	sample_format = sample_format_from_oss(oss_format);

	if (sample_format == SampleFormat::UNDEFINED)
		return false;

	*sample_format_r = sample_format;
	*oss_format_r = oss_format;

	PcmExport::Params params;
	params.alsa_channel_order = true;
#ifdef AFMT_S24_PACKED
	params.pack24 = oss_format == AFMT_S24_PACKED;
	params.reverse_endian = oss_format == AFMT_S24_PACKED &&
		!IsLittleEndian();
#endif

	pcm_export.Open(sample_format, 0, params);

	return true;
}

/**
 * Set up the sample format, and attempts to find alternatives if the
 * specified format is not supported.
 */
static void
oss_setup_sample_format(FileDescriptor fd, AudioFormat &audio_format,
			int *oss_format_r,
			PcmExport &pcm_export)
{
	SampleFormat mpd_format;
	if (oss_probe_sample_format(fd, audio_format.format,
				    &mpd_format, oss_format_r,
				    pcm_export)) {
		audio_format.format = mpd_format;
		return;
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

		if (oss_probe_sample_format(fd, mpd_format,
					    &mpd_format, oss_format_r,
					    pcm_export)) {
			audio_format.format = mpd_format;
			return;
		}
	}

	throw std::runtime_error("Failed to set sample format");
}

inline void
OssOutput::Setup(AudioFormat &_audio_format)
{
	oss_setup_channels(fd, _audio_format, effective_channels);
	oss_setup_sample_rate(fd, _audio_format, effective_speed);
	oss_setup_sample_format(fd, _audio_format, &effective_samplesize,
				pcm_export);
}

#ifdef ENABLE_OSS_DSD

void
OssOutput::SetupDop(const AudioFormat &audio_format)
{
	assert(audio_format.format == SampleFormat::DSD);

	effective_channels = audio_format.channels;

	/* DoP packs two 8-bit "samples" in one 24-bit "sample" */
	effective_speed = audio_format.sample_rate / 2;

	effective_samplesize = AFMT_S32_NE;

	OssIoctlExact(fd, SNDCTL_DSP_CHANNELS, effective_channels,
		      "Failed to set channel count");
	OssIoctlExact(fd, SNDCTL_DSP_SPEED, effective_speed,
		      "Failed to set sample rate");
	OssIoctlExact(fd, SNDCTL_DSP_SAMPLESIZE, effective_samplesize,
		      "Failed to set sample format");

	PcmExport::Params params;
	params.alsa_channel_order = true;
	params.dsd_mode = PcmExport::DsdMode::DOP;
	params.shift8 = true;

	pcm_export->Open(audio_format.format, audio_format.channels, params);
}

#endif

void
OssOutput::SetupOrDop(AudioFormat &audio_format)
{
#ifdef ENABLE_OSS_DSD
	std::exception_ptr dop_error;
	if (dop_setting && audio_format.format == SampleFormat::DSD) {
		try {
			SetupDop(audio_format);
			return;
		} catch (...) {
			dop_error = std::current_exception();
		}
	}

	try {
#endif
		Setup(audio_format);
#ifdef ENABLE_OSS_DSD
	} catch (...) {
		if (dop_error)
			/* if DoP was attempted, prefer returning the
			   original DoP error instead of the fallback
			   error */
			std::rethrow_exception(dop_error);
		else
			throw;
	}
#endif
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

	OssIoctlExact(fd, SNDCTL_DSP_CHANNELS, effective_channels,
		      "Failed to set channel count");
	OssIoctlExact(fd, SNDCTL_DSP_SPEED, effective_speed,
		      "Failed to set sample rate");
	OssIoctlExact(fd, SNDCTL_DSP_SAMPLESIZE, effective_samplesize,
		      "Failed to set sample format");
} catch (...) {
	DoClose();
	throw;
}

void
OssOutput::Open(AudioFormat &_audio_format)
try {
	if (!fd.Open(device, O_WRONLY))
		throw FormatErrno("Error opening OSS device \"%s\"", device);

	SetupOrDop(_audio_format);
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

	pcm_export->Reset();
}

size_t
OssOutput::Play(const void *chunk, size_t size)
{
	ssize_t ret;

	assert(size > 0);

	/* reopen the device since it was closed by dropBufferedAudio */
	if (!fd.IsDefined())
		Reopen();

	const auto e = pcm_export->Export({chunk, size});
	if (e.empty())
		return size;

	chunk = e.data;
	size = e.size;

	while (true) {
		ret = fd.Write(chunk, size);
		if (ret > 0)
			return pcm_export->CalcInputSize(ret);

		if (ret < 0 && errno != EINTR)
			throw FormatErrno("Write error on %s", device);
	}
}

constexpr struct AudioOutputPlugin oss_output_plugin = {
	"oss",
	oss_output_test_default_device,
	OssOutput::Create,
	&oss_mixer_plugin,
};
