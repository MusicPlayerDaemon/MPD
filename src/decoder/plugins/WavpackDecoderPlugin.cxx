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
#include "WavpackDecoderPlugin.hxx"
#include "../DecoderAPI.hxx"
#include "input/InputStream.hxx"
#include "CheckAudioFormat.hxx"
#include "tag/TagHandler.hxx"
#include "tag/ApeTag.hxx"
#include "fs/Path.hxx"
#include "util/Error.hxx"
#include "util/Domain.hxx"
#include "util/Macros.hxx"
#include "Log.hxx"

#include <wavpack/wavpack.h>
#include <glib.h>

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#define ERRORLEN 80

static constexpr Domain wavpack_domain("wavpack");

/** A pointer type for format converter function. */
typedef void (*format_samples_t)(
	int bytes_per_sample,
	void *buffer, uint32_t count
);

/*
 * This function has been borrowed from the tiny player found on
 * wavpack.com. Modifications were required because mpd only handles
 * max 24-bit samples.
 */
static void
format_samples_int(int bytes_per_sample, void *buffer, uint32_t count)
{
	int32_t *src = (int32_t *)buffer;

	switch (bytes_per_sample) {
	case 1: {
		int8_t *dst = (int8_t *)buffer;
		/*
		 * The asserts like the following one are because we do the
		 * formatting of samples within a single buffer. The size
		 * of the output samples never can be greater than the size
		 * of the input ones. Otherwise we would have an overflow.
		 */
		static_assert(sizeof(*dst) <= sizeof(*src), "Wrong size");

		/* pass through and align 8-bit samples */
		while (count--) {
			*dst++ = *src++;
		}
		break;
	}
	case 2: {
		uint16_t *dst = (uint16_t *)buffer;
		static_assert(sizeof(*dst) <= sizeof(*src), "Wrong size");

		/* pass through and align 16-bit samples */
		while (count--) {
			*dst++ = *src++;
		}
		break;
	}

	case 3:
	case 4:
		/* do nothing */
		break;
	}
}

/*
 * This function converts floating point sample data to 24-bit integer.
 */
static void
format_samples_float(gcc_unused int bytes_per_sample, void *buffer,
		     uint32_t count)
{
	float *p = (float *)buffer;

	while (count--) {
		*p /= (1 << 23);
		++p;
	}
}

/**
 * Choose a MPD sample format from libwavpacks' number of bits.
 */
static SampleFormat
wavpack_bits_to_sample_format(bool is_float, int bytes_per_sample)
{
	if (is_float)
		return SampleFormat::FLOAT;

	switch (bytes_per_sample) {
	case 1:
		return SampleFormat::S8;

	case 2:
		return SampleFormat::S16;

	case 3:
		return SampleFormat::S24_P32;

	case 4:
		return SampleFormat::S32;

	default:
		return SampleFormat::UNDEFINED;
	}
}

/*
 * This does the main decoding thing.
 * Requires an already opened WavpackContext.
 */
static void
wavpack_decode(Decoder &decoder, WavpackContext *wpc, bool can_seek)
{
	bool is_float = (WavpackGetMode(wpc) & MODE_FLOAT) != 0;
	SampleFormat sample_format =
		wavpack_bits_to_sample_format(is_float,
					      WavpackGetBytesPerSample(wpc));

	Error error;
	AudioFormat audio_format;
	if (!audio_format_init_checked(audio_format,
				       WavpackGetSampleRate(wpc),
				       sample_format,
				       WavpackGetNumChannels(wpc), error)) {
		LogError(error);
		return;
	}

	const format_samples_t format_samples = is_float
		? format_samples_float
		: format_samples_int;

	const auto total_time =
		SongTime::FromScale<uint64_t>(WavpackGetNumSamples(wpc),
					      audio_format.sample_rate);

	const int bytes_per_sample = WavpackGetBytesPerSample(wpc);
	const int output_sample_size = audio_format.GetFrameSize();

	/* wavpack gives us all kind of samples in a 32-bit space */
	int32_t chunk[1024];
	const uint32_t samples_requested = ARRAY_SIZE(chunk) /
		audio_format.channels;

	decoder_initialized(decoder, audio_format, can_seek, total_time);

	DecoderCommand cmd = decoder_get_command(decoder);
	while (cmd != DecoderCommand::STOP) {
		if (cmd == DecoderCommand::SEEK) {
			if (can_seek) {
				auto where = decoder_seek_where_frame(decoder);

				if (WavpackSeekSample(wpc, where)) {
					decoder_command_finished(decoder);
				} else {
					decoder_seek_error(decoder);
				}
			} else {
				decoder_seek_error(decoder);
			}
		}

		uint32_t samples_got = WavpackUnpackSamples(wpc, chunk,
							    samples_requested);
		if (samples_got == 0)
			break;

		int bitrate = (int)(WavpackGetInstantBitrate(wpc) / 1000 +
				    0.5);
		format_samples(bytes_per_sample, chunk,
			       samples_got * audio_format.channels);

		cmd = decoder_data(decoder, nullptr, chunk,
				   samples_got * output_sample_size,
				   bitrate);
	}
}

/**
 * Locate and parse a floating point tag.  Returns true if it was
 * found.
 */
static bool
wavpack_tag_float(WavpackContext *wpc, const char *key, float *value_r)
{
	char buffer[64];
	if (WavpackGetTagItem(wpc, key, buffer, sizeof(buffer)) <= 0)
		return false;

	*value_r = atof(buffer);
	return true;
}

static bool
wavpack_replaygain(ReplayGainInfo &rgi,
		   WavpackContext *wpc)
{
	rgi.Clear();

	bool found = false;
	found |= wavpack_tag_float(wpc, "replaygain_track_gain",
				   &rgi.tuples[REPLAY_GAIN_TRACK].gain);
	found |= wavpack_tag_float(wpc, "replaygain_track_peak",
				   &rgi.tuples[REPLAY_GAIN_TRACK].peak);
	found |= wavpack_tag_float(wpc, "replaygain_album_gain",
				   &rgi.tuples[REPLAY_GAIN_ALBUM].gain);
	found |= wavpack_tag_float(wpc, "replaygain_album_peak",
				   &rgi.tuples[REPLAY_GAIN_ALBUM].peak);

	return found;
}

static void
wavpack_scan_tag_item(WavpackContext *wpc, const char *name,
		      TagType type,
		      const struct tag_handler *handler, void *handler_ctx)
{
	char buffer[1024];
	int len = WavpackGetTagItem(wpc, name, buffer, sizeof(buffer));
	if (len <= 0 || (unsigned)len >= sizeof(buffer))
		return;

	tag_handler_invoke_tag(handler, handler_ctx, type, buffer);

}

static void
wavpack_scan_pair(WavpackContext *wpc, const char *name,
		  const struct tag_handler *handler, void *handler_ctx)
{
	char buffer[8192];
	int len = WavpackGetTagItem(wpc, name, buffer, sizeof(buffer));
	if (len <= 0 || (unsigned)len >= sizeof(buffer))
		return;

	tag_handler_invoke_pair(handler, handler_ctx, name, buffer);
}

/*
 * Reads metainfo from the specified file.
 */
static bool
wavpack_scan_file(Path path_fs,
		  const struct tag_handler *handler, void *handler_ctx)
{
	char error[ERRORLEN];
	WavpackContext *wpc = WavpackOpenFileInput(path_fs.c_str(), error,
						   OPEN_TAGS, 0);
	if (wpc == nullptr) {
		FormatError(wavpack_domain,
			    "failed to open WavPack file \"%s\": %s",
			    path_fs.c_str(), error);
		return false;
	}

	const auto duration =
		SongTime::FromScale<uint64_t>(WavpackGetNumSamples(wpc),
					      WavpackGetSampleRate(wpc));
	tag_handler_invoke_duration(handler, handler_ctx, duration);

	/* the WavPack format implies APEv2 tags, which means we can
	   reuse the mapping from tag_ape.c */

	for (unsigned i = 0; i < TAG_NUM_OF_ITEM_TYPES; ++i) {
		const char *name = tag_item_names[i];
		if (name != nullptr)
			wavpack_scan_tag_item(wpc, name, (TagType)i,
					      handler, handler_ctx);
	}

	for (const struct tag_table *i = ape_tags; i->name != nullptr; ++i)
		wavpack_scan_tag_item(wpc, i->name, i->type,
				      handler, handler_ctx);

	if (handler->pair != nullptr) {
		char name[64];

		for (int i = 0, n = WavpackGetNumTagItems(wpc);
		     i < n; ++i) {
			int len = WavpackGetTagItemIndexed(wpc, i, name,
							   sizeof(name));
			if (len <= 0 || (unsigned)len >= sizeof(name))
				continue;

			wavpack_scan_pair(wpc, name, handler, handler_ctx);
		}
	}

	WavpackCloseFile(wpc);

	return true;
}

/*
 * mpd input_stream <=> WavpackStreamReader wrapper callbacks
 */

/* This struct is needed for per-stream last_byte storage. */
struct WavpackInput {
	Decoder &decoder;
	InputStream &is;
	/* Needed for push_back_byte() */
	int last_byte;

	constexpr WavpackInput(Decoder &_decoder, InputStream &_is)
		:decoder(_decoder), is(_is), last_byte(EOF) {}

	int32_t ReadBytes(void *data, size_t bcount);
};

/**
 * Little wrapper for struct WavpackInput to cast from void *.
 */
static WavpackInput *
wpin(void *id)
{
	assert(id);
	return (WavpackInput *)id;
}

static int32_t
wavpack_input_read_bytes(void *id, void *data, int32_t bcount)
{
	return wpin(id)->ReadBytes(data, bcount);
}

int32_t
WavpackInput::ReadBytes(void *data, size_t bcount)
{
	uint8_t *buf = (uint8_t *)data;
	int32_t i = 0;

	if (last_byte != EOF) {
		*buf++ = last_byte;
		last_byte = EOF;
		--bcount;
		++i;
	}

	/* wavpack fails if we return a partial read, so we just wait
	   until the buffer is full */
	while (bcount > 0) {
		size_t nbytes = decoder_read(&decoder, is, buf, bcount);
		if (nbytes == 0) {
			/* EOF, error or a decoder command */
			break;
		}

		i += nbytes;
		bcount -= nbytes;
		buf += nbytes;
	}

	return i;
}

static uint32_t
wavpack_input_get_pos(void *id)
{
	WavpackInput &wpi = *wpin(id);

	return wpi.is.GetOffset();
}

static int
wavpack_input_set_pos_abs(void *id, uint32_t pos)
{
	WavpackInput &wpi = *wpin(id);

	return wpi.is.LockSeek(pos, IgnoreError()) ? 0 : -1;
}

static int
wavpack_input_set_pos_rel(void *id, int32_t delta, int mode)
{
	WavpackInput &wpi = *wpin(id);
	InputStream &is = wpi.is;

	offset_type offset = delta;
	switch (mode) {
	case SEEK_SET:
		break;

	case SEEK_CUR:
		offset += is.GetOffset();
		break;

	case SEEK_END:
		if (!is.KnownSize())
			return -1;

		offset += is.GetSize();
		break;

	default:
		return -1;
	}

	return is.LockSeek(offset, IgnoreError()) ? 0 : -1;
}

static int
wavpack_input_push_back_byte(void *id, int c)
{
	WavpackInput &wpi = *wpin(id);

	if (wpi.last_byte == EOF) {
		wpi.last_byte = c;
		return c;
	} else {
		return EOF;
	}
}

static uint32_t
wavpack_input_get_length(void *id)
{
	WavpackInput &wpi = *wpin(id);
	InputStream &is = wpi.is;

	if (!is.KnownSize())
		return 0;

	return is.GetSize();
}

static int
wavpack_input_can_seek(void *id)
{
	WavpackInput &wpi = *wpin(id);
	InputStream &is = wpi.is;

	return is.IsSeekable();
}

static WavpackStreamReader mpd_is_reader = {
	wavpack_input_read_bytes,
	wavpack_input_get_pos,
	wavpack_input_set_pos_abs,
	wavpack_input_set_pos_rel,
	wavpack_input_push_back_byte,
	wavpack_input_get_length,
	wavpack_input_can_seek,
	nullptr /* no need to write edited tags */
};

static WavpackInput *
wavpack_open_wvc(Decoder &decoder, const char *uri)
{
	/*
	 * As we use dc->utf8url, this function will be bad for
	 * single files. utf8url is not absolute file path :/
	 */
	if (uri == nullptr)
		return nullptr;

	char *wvc_url = g_strconcat(uri, "c", nullptr);

	InputStream *is_wvc = decoder_open_uri(decoder, uri, IgnoreError());
	g_free(wvc_url);

	if (is_wvc == nullptr)
		return nullptr;

	return new WavpackInput(decoder, *is_wvc);
}

/*
 * Decodes a stream.
 */
static void
wavpack_streamdecode(Decoder &decoder, InputStream &is)
{
	int open_flags = OPEN_NORMALIZE;
	bool can_seek = is.IsSeekable();

	WavpackInput *wvc = wavpack_open_wvc(decoder, is.GetURI());
	if (wvc != nullptr) {
		open_flags |= OPEN_WVC;
		can_seek &= wvc->is.IsSeekable();
	}

	if (!can_seek) {
		open_flags |= OPEN_STREAMING;
	}

	WavpackInput isp(decoder, is);

	char error[ERRORLEN];
	WavpackContext *wpc =
		WavpackOpenFileInputEx(&mpd_is_reader, &isp, wvc,
				       error, open_flags, 23);

	if (wpc == nullptr) {
		FormatError(wavpack_domain,
			    "failed to open WavPack stream: %s", error);
		return;
	}

	wavpack_decode(decoder, wpc, can_seek);

	WavpackCloseFile(wpc);

	if (wvc != nullptr) {
		delete &wvc->is;
		delete wvc;
	}
}

/*
 * Decodes a file.
 */
static void
wavpack_filedecode(Decoder &decoder, Path path_fs)
{
	char error[ERRORLEN];
	WavpackContext *wpc = WavpackOpenFileInput(path_fs.c_str(), error,
						   OPEN_TAGS | OPEN_WVC | OPEN_NORMALIZE,
						   23);
	if (wpc == nullptr) {
		FormatWarning(wavpack_domain,
			      "failed to open WavPack file \"%s\": %s",
			      path_fs.c_str(), error);
		return;
	}

	ReplayGainInfo rgi;
	if (wavpack_replaygain(rgi, wpc))
		decoder_replay_gain(decoder, &rgi);

	wavpack_decode(decoder, wpc, true);

	WavpackCloseFile(wpc);
}

static char const *const wavpack_suffixes[] = {
	"wv",
	nullptr
};

static char const *const wavpack_mime_types[] = {
	"audio/x-wavpack",
	nullptr
};

const struct DecoderPlugin wavpack_decoder_plugin = {
	"wavpack",
	nullptr,
	nullptr,
	wavpack_streamdecode,
	wavpack_filedecode,
	wavpack_scan_file,
	nullptr,
	nullptr,
	wavpack_suffixes,
	wavpack_mime_types
};
