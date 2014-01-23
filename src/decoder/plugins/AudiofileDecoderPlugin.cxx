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
#include "InputStream.hxx"
#include "CheckAudioFormat.hxx"
#include "tag/TagHandler.hxx"
#include "util/Error.hxx"
#include "util/Domain.hxx"
#include "Log.hxx"

#include <audiofile.h>
#include <af_vfs.h>

#include <assert.h>

/* pick 1020 since its devisible for 8,16,24, and 32-bit audio */
#define CHUNK_SIZE		1020

static constexpr Domain audiofile_domain("audiofile");

static int audiofile_get_duration(const char *file)
{
	int total_time;
	AFfilehandle af_fp = afOpenFile(file, "r", nullptr);
	if (af_fp == AF_NULL_FILEHANDLE) {
		return -1;
	}
	total_time = (int)
	    ((double)afGetFrameCount(af_fp, AF_DEFAULT_TRACK)
	     / afGetRate(af_fp, AF_DEFAULT_TRACK));
	afCloseFile(af_fp);
	return total_time;
}

static ssize_t
audiofile_file_read(AFvirtualfile *vfile, void *data, size_t length)
{
	InputStream &is = *(InputStream *)vfile->closure;

	Error error;
	size_t nbytes = is.LockRead(data, length, error);
	if (nbytes == 0 && error.IsDefined()) {
		LogError(error);
		return -1;
	}

	return nbytes;
}

static AFfileoffset
audiofile_file_length(AFvirtualfile *vfile)
{
	InputStream &is = *(InputStream *)vfile->closure;
	return is.GetSize();
}

static AFfileoffset
audiofile_file_tell(AFvirtualfile *vfile)
{
	InputStream &is = *(InputStream *)vfile->closure;
	return is.GetOffset();
}

static void
audiofile_file_destroy(AFvirtualfile *vfile)
{
	assert(vfile->closure != nullptr);

	vfile->closure = nullptr;
}

static AFfileoffset
audiofile_file_seek(AFvirtualfile *vfile, AFfileoffset offset, int is_relative)
{
	InputStream &is = *(InputStream *)vfile->closure;
	int whence = (is_relative ? SEEK_CUR : SEEK_SET);

	Error error;
	if (is.LockSeek(offset, whence, error)) {
		return is.GetOffset();
	} else {
		return -1;
	}
}

static AFvirtualfile *
setup_virtual_fops(InputStream &stream)
{
	AFvirtualfile *vf = new AFvirtualfile();
	vf->closure = &stream;
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
	AFvirtualfile *vf;
	int fs, frame_count;
	AFfilehandle af_fp;
	AudioFormat audio_format;
	float total_time;
	uint16_t bit_rate;
	int ret;
	char chunk[CHUNK_SIZE];

	if (!is.IsSeekable()) {
		LogWarning(audiofile_domain, "not seekable");
		return;
	}

	vf = setup_virtual_fops(is);

	af_fp = afOpenVirtualFile(vf, "r", nullptr);
	if (af_fp == AF_NULL_FILEHANDLE) {
		LogWarning(audiofile_domain, "failed to input stream");
		return;
	}

	Error error;
	if (!audio_format_init_checked(audio_format,
				       afGetRate(af_fp, AF_DEFAULT_TRACK),
				       audiofile_setup_sample_format(af_fp),
				       afGetVirtualChannels(af_fp, AF_DEFAULT_TRACK),
				       error)) {
		LogError(error);
		afCloseFile(af_fp);
		return;
	}

	frame_count = afGetFrameCount(af_fp, AF_DEFAULT_TRACK);

	total_time = ((float)frame_count / (float)audio_format.sample_rate);

	bit_rate = (uint16_t)(is.GetSize() * 8.0 / total_time / 1000.0 + 0.5);

	fs = (int)afGetVirtualFrameSize(af_fp, AF_DEFAULT_TRACK, 1);

	decoder_initialized(decoder, audio_format, true, total_time);

	DecoderCommand cmd;
	do {
		ret = afReadFrames(af_fp, AF_DEFAULT_TRACK, chunk,
				   CHUNK_SIZE / fs);
		if (ret <= 0)
			break;

		cmd = decoder_data(decoder, nullptr,
				   chunk, ret * fs,
				   bit_rate);

		if (cmd == DecoderCommand::SEEK) {
			AFframecount frame = decoder_seek_where(decoder) *
				audio_format.sample_rate;
			afSeekFrame(af_fp, AF_DEFAULT_TRACK, frame);

			decoder_command_finished(decoder);
			cmd = DecoderCommand::NONE;
		}
	} while (cmd == DecoderCommand::NONE);

	afCloseFile(af_fp);
}

static bool
audiofile_scan_file(const char *file,
		    const struct tag_handler *handler, void *handler_ctx)
{
	int total_time = audiofile_get_duration(file);

	if (total_time < 0) {
		FormatWarning(audiofile_domain,
			      "Failed to get total song time from: %s",
			      file);
		return false;
	}

	tag_handler_invoke_duration(handler, handler_ctx, total_time);
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
	nullptr,
	nullptr,
	audiofile_stream_decode,
	nullptr,
	audiofile_scan_file,
	nullptr,
	nullptr,
	audiofile_suffixes,
	audiofile_mime_types,
};
