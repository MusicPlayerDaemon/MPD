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
#include "AudiofileDecoderPlugin.hxx"
#include "../DecoderAPI.hxx"
#include "input/InputStream.hxx"
#include "CheckAudioFormat.hxx"
#include "tag/TagHandler.hxx"
#include "fs/Path.hxx"
#include "util/Error.hxx"
#include "util/Domain.hxx"
#include "Log.hxx"

#include <audiofile.h>
#include <af_vfs.h>

#include <assert.h>
#include <stdio.h>

static constexpr Domain audiofile_domain("audiofile");

static void
audiofile_error_func(long, const char *msg)
{
	LogWarning(audiofile_domain, msg);
}

static bool
audiofile_init(const config_param &)
{
	afSetErrorHandler(audiofile_error_func);
	return true;
}

struct AudioFileInputStream {
	Decoder *const decoder;
	InputStream &is;

	size_t Read(void *buffer, size_t size) {
		/* libaudiofile does not like partial reads at all,
		   and will abort playback; therefore always force full
		   reads */
		return decoder_read_full(decoder, is, buffer, size)
			? size
			: 0;
	}
};

gcc_pure
static SongTime
audiofile_get_duration(AFfilehandle fh)
{
	return SongTime::FromScale<uint64_t>(afGetFrameCount(fh, AF_DEFAULT_TRACK),
					     afGetRate(fh, AF_DEFAULT_TRACK));
}

static ssize_t
audiofile_file_read(AFvirtualfile *vfile, void *data, size_t length)
{
	AudioFileInputStream &afis = *(AudioFileInputStream *)vfile->closure;

	return afis.Read(data, length);
}

static AFfileoffset
audiofile_file_length(AFvirtualfile *vfile)
{
	AudioFileInputStream &afis = *(AudioFileInputStream *)vfile->closure;
	InputStream &is = afis.is;

	return is.GetSize();
}

static AFfileoffset
audiofile_file_tell(AFvirtualfile *vfile)
{
	AudioFileInputStream &afis = *(AudioFileInputStream *)vfile->closure;
	InputStream &is = afis.is;

	return is.GetOffset();
}

static void
audiofile_file_destroy(AFvirtualfile *vfile)
{
	assert(vfile->closure != nullptr);

	vfile->closure = nullptr;
}

static AFfileoffset
audiofile_file_seek(AFvirtualfile *vfile, AFfileoffset _offset,
		    int is_relative)
{
	AudioFileInputStream &afis = *(AudioFileInputStream *)vfile->closure;
	InputStream &is = afis.is;

	offset_type offset = _offset;
	if (is_relative)
		offset += is.GetOffset();

	Error error;
	if (is.LockSeek(offset, error)) {
		return is.GetOffset();
	} else {
		LogError(error, "Seek failed");
		return -1;
	}
}

static AFvirtualfile *
setup_virtual_fops(AudioFileInputStream &afis)
{
	AFvirtualfile *vf = new AFvirtualfile();
	vf->closure = &afis;
	vf->write = nullptr;
	vf->read    = audiofile_file_read;
	vf->length  = audiofile_file_length;
	vf->destroy = audiofile_file_destroy;
	vf->seek    = audiofile_file_seek;
	vf->tell    = audiofile_file_tell;
	return vf;
}

static SampleFormat
audiofile_bits_to_sample_format(int bits)
{
	switch (bits) {
	case 8:
		return SampleFormat::S8;

	case 16:
		return SampleFormat::S16;

	case 24:
		return SampleFormat::S24_P32;

	case 32:
		return SampleFormat::S32;
	}

	return SampleFormat::UNDEFINED;
}

static SampleFormat
audiofile_setup_sample_format(AFfilehandle af_fp)
{
	int fs, bits;

	afGetSampleFormat(af_fp, AF_DEFAULT_TRACK, &fs, &bits);
	if (!audio_valid_sample_format(audiofile_bits_to_sample_format(bits))) {
		FormatDebug(audiofile_domain,
			    "input file has %d bit samples, converting to 16",
			    bits);
		bits = 16;
	}

	afSetVirtualSampleFormat(af_fp, AF_DEFAULT_TRACK,
				 AF_SAMPFMT_TWOSCOMP, bits);
	afGetVirtualSampleFormat(af_fp, AF_DEFAULT_TRACK, &fs, &bits);

	return audiofile_bits_to_sample_format(bits);
}

static void
audiofile_stream_decode(Decoder &decoder, InputStream &is)
{
	if (!is.IsSeekable() || !is.KnownSize()) {
		LogWarning(audiofile_domain, "not seekable");
		return;
	}

	AudioFileInputStream afis{&decoder, is};
	AFvirtualfile *const vf = setup_virtual_fops(afis);

	const AFfilehandle fh = afOpenVirtualFile(vf, "r", nullptr);
	if (fh == AF_NULL_FILEHANDLE)
		return;

	Error error;
	AudioFormat audio_format;
	if (!audio_format_init_checked(audio_format,
				       afGetRate(fh, AF_DEFAULT_TRACK),
				       audiofile_setup_sample_format(fh),
				       afGetVirtualChannels(fh, AF_DEFAULT_TRACK),
				       error)) {
		LogError(error);
		afCloseFile(fh);
		return;
	}

	const auto total_time = audiofile_get_duration(fh);

	const uint16_t kbit_rate = (uint16_t)
		(is.GetSize() * uint64_t(8) / total_time.ToMS());

	const unsigned frame_size = (unsigned)
		afGetVirtualFrameSize(fh, AF_DEFAULT_TRACK, true);

	decoder_initialized(decoder, audio_format, true, total_time);

	DecoderCommand cmd;
	do {
		/* pick 1020 since its divisible for 8,16,24, and
		   32-bit audio */
		char chunk[1020];
		const int nframes =
			afReadFrames(fh, AF_DEFAULT_TRACK, chunk,
				     sizeof(chunk) / frame_size);
		if (nframes <= 0)
			break;

		cmd = decoder_data(decoder, nullptr,
				   chunk, nframes * frame_size,
				   kbit_rate);

		if (cmd == DecoderCommand::SEEK) {
			AFframecount frame = decoder_seek_where_frame(decoder);
			afSeekFrame(fh, AF_DEFAULT_TRACK, frame);

			decoder_command_finished(decoder);
			cmd = DecoderCommand::NONE;
		}
	} while (cmd == DecoderCommand::NONE);

	afCloseFile(fh);
}

gcc_pure
static SignedSongTime
audiofile_get_duration(InputStream &is)
{
	if (!is.IsSeekable() || !is.KnownSize())
		return SignedSongTime::Negative();

	AudioFileInputStream afis{nullptr, is};
	AFvirtualfile *vf = setup_virtual_fops(afis);
	AFfilehandle fh = afOpenVirtualFile(vf, "r", nullptr);
	if (fh == AF_NULL_FILEHANDLE)
		return SignedSongTime::Negative();

	const auto duration = audiofile_get_duration(fh);
	afCloseFile(fh);
	return duration;
}

static bool
audiofile_scan_stream(InputStream &is,
		      const struct tag_handler *handler, void *handler_ctx)
{
	const auto duration = audiofile_get_duration(is);
	if (duration.IsNegative())
		return false;

	tag_handler_invoke_duration(handler, handler_ctx, SongTime(duration));
	return true;
}

static const char *const audiofile_suffixes[] = {
	"wav", "au", "aiff", "aif", nullptr
};

static const char *const audiofile_mime_types[] = {
	"audio/x-wav",
	"audio/x-aiff",
	nullptr
};

const struct DecoderPlugin audiofile_decoder_plugin = {
	"audiofile",
	audiofile_init,
	nullptr,
	audiofile_stream_decode,
	nullptr,
	nullptr,
	audiofile_scan_stream,
	nullptr,
	audiofile_suffixes,
	audiofile_mime_types,
};
