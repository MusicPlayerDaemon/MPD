/*
 * Copyright (C) 2003-2013 The Music Player Daemon Project
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

/* necessary because libavutil/common.h uses UINT64_C */
#define __STDC_CONSTANT_MACROS

#include "config.h"
#include "FfmpegDecoderPlugin.hxx"
#include "DecoderAPI.hxx"
#include "FfmpegMetaData.hxx"
#include "TagHandler.hxx"
#include "InputStream.hxx"
#include "CheckAudioFormat.hxx"

#include <glib.h>

#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavformat/avio.h>
#include <libavutil/avutil.h>
#include <libavutil/log.h>
#include <libavutil/mathematics.h>
#include <libavutil/dict.h>
}

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "ffmpeg"

/* suppress the ffmpeg compatibility macro */
#ifdef SampleFormat
#undef SampleFormat
#endif

static GLogLevelFlags
level_ffmpeg_to_glib(int level)
{
	if (level <= AV_LOG_FATAL)
		return G_LOG_LEVEL_CRITICAL;

	if (level <= AV_LOG_ERROR)
		return G_LOG_LEVEL_WARNING;

	if (level <= AV_LOG_INFO)
		return G_LOG_LEVEL_MESSAGE;

	return G_LOG_LEVEL_DEBUG;
}

static void
mpd_ffmpeg_log_callback(gcc_unused void *ptr, int level,
			const char *fmt, va_list vl)
{
	const AVClass * cls = NULL;

	if (ptr != NULL)
		cls = *(const AVClass *const*)ptr;

	if (cls != NULL) {
		char *domain = g_strconcat(G_LOG_DOMAIN, "/", cls->item_name(ptr), NULL);
		g_logv(domain, level_ffmpeg_to_glib(level), fmt, vl);
		g_free(domain);
	}
}

struct AvioStream {
	struct decoder *decoder;
	struct input_stream *input;

	AVIOContext *io;

	unsigned char buffer[8192];

	AvioStream(struct decoder *_decoder, input_stream *_input)
		:decoder(_decoder), input(_input), io(nullptr) {}

	~AvioStream() {
		if (io != nullptr)
			av_free(io);
	}

	bool Open();
};

static int
mpd_ffmpeg_stream_read(void *opaque, uint8_t *buf, int size)
{
	AvioStream *stream = (AvioStream *)opaque;

	return decoder_read(stream->decoder, stream->input,
			    (void *)buf, size);
}

static int64_t
mpd_ffmpeg_stream_seek(void *opaque, int64_t pos, int whence)
{
	AvioStream *stream = (AvioStream *)opaque;

	if (whence == AVSEEK_SIZE)
		return stream->input->size;

	if (!input_stream_lock_seek(stream->input, pos, whence, NULL))
		return -1;

	return stream->input->offset;
}

bool
AvioStream::Open()
{
	io = avio_alloc_context(buffer, sizeof(buffer),
				false, this,
				mpd_ffmpeg_stream_read, nullptr,
				input->seekable
				? mpd_ffmpeg_stream_seek : nullptr);
	return io != nullptr;
}

/**
 * API compatibility wrapper for av_open_input_stream() and
 * avformat_open_input().
 */
static int
mpd_ffmpeg_open_input(AVFormatContext **ic_ptr,
		      AVIOContext *pb,
		      const char *filename,
		      AVInputFormat *fmt)
{
	AVFormatContext *context = avformat_alloc_context();
	if (context == NULL)
		return AVERROR(ENOMEM);

	context->pb = pb;
	*ic_ptr = context;
	return avformat_open_input(ic_ptr, filename, fmt, NULL);
}

static bool
ffmpeg_init(gcc_unused const config_param &param)
{
	av_log_set_callback(mpd_ffmpeg_log_callback);

	av_register_all();
	return true;
}

static int
ffmpeg_find_audio_stream(const AVFormatContext *format_context)
{
	for (unsigned i = 0; i < format_context->nb_streams; ++i)
		if (format_context->streams[i]->codec->codec_type ==
		    AVMEDIA_TYPE_AUDIO)
			return i;

	return -1;
}

gcc_const
static double
time_from_ffmpeg(int64_t t, const AVRational time_base)
{
	assert(t != (int64_t)AV_NOPTS_VALUE);

	return (double)av_rescale_q(t, time_base, (AVRational){1, 1024})
		/ (double)1024;
}

gcc_const
static int64_t
time_to_ffmpeg(double t, const AVRational time_base)
{
	return av_rescale_q((int64_t)(t * 1024), (AVRational){1, 1024},
			    time_base);
}

static void
copy_interleave_frame2(uint8_t *dest, uint8_t **src,
		       unsigned nframes, unsigned nchannels,
		       unsigned sample_size)
{
	for (unsigned frame = 0; frame < nframes; ++frame) {
		for (unsigned channel = 0; channel < nchannels; ++channel) {
			memcpy(dest, src[channel] + frame * sample_size,
			       sample_size);
			dest += sample_size;
		}
	}
}

/**
 * Copy PCM data from a AVFrame to an interleaved buffer.
 */
static int
copy_interleave_frame(const AVCodecContext *codec_context,
		      const AVFrame *frame,
		      uint8_t *buffer, size_t buffer_size)
{
	int plane_size;
	const int data_size =
		av_samples_get_buffer_size(&plane_size,
					   codec_context->channels,
					   frame->nb_samples,
					   codec_context->sample_fmt, 1);
	if (buffer_size < (size_t)data_size)
		/* buffer is too small - shouldn't happen */
		return AVERROR(EINVAL);

	if (av_sample_fmt_is_planar(codec_context->sample_fmt) &&
	    codec_context->channels > 1) {
		copy_interleave_frame2(buffer, frame->extended_data,
				       frame->nb_samples,
				       codec_context->channels,
				       av_get_bytes_per_sample(codec_context->sample_fmt));
	} else {
		memcpy(buffer, frame->extended_data[0], data_size);
	}

	return data_size;
}

static enum decoder_command
ffmpeg_send_packet(struct decoder *decoder, struct input_stream *is,
		   const AVPacket *packet,
		   AVCodecContext *codec_context,
		   const AVRational *time_base,
		   AVFrame *frame)
{
	if (packet->pts >= 0 && packet->pts != (int64_t)AV_NOPTS_VALUE)
		decoder_timestamp(decoder,
				  time_from_ffmpeg(packet->pts, *time_base));

	AVPacket packet2 = *packet;

	uint8_t aligned_buffer[(AVCODEC_MAX_AUDIO_FRAME_SIZE * 3) / 2 + 16];
	const size_t buffer_size = sizeof(aligned_buffer);

	enum decoder_command cmd = DECODE_COMMAND_NONE;
	while (packet2.size > 0 &&
	       cmd == DECODE_COMMAND_NONE) {
		int audio_size = buffer_size;
		int got_frame = 0;
		int len = avcodec_decode_audio4(codec_context,
						frame, &got_frame,
						&packet2);
		if (len >= 0 && got_frame) {
			audio_size = copy_interleave_frame(codec_context,
							   frame,
							   aligned_buffer,
							   buffer_size);
			if (audio_size < 0)
				len = audio_size;
		} else if (len >= 0)
			len = -1;

		if (len < 0) {
			/* if error, we skip the frame */
			g_message("decoding failed, frame skipped\n");
			break;
		}

		packet2.data += len;
		packet2.size -= len;

		if (audio_size <= 0)
			continue;

		cmd = decoder_data(decoder, is,
				   aligned_buffer, audio_size,
				   codec_context->bit_rate / 1000);
	}
	return cmd;
}

gcc_const
static SampleFormat
ffmpeg_sample_format(enum AVSampleFormat sample_fmt)
{
	switch (sample_fmt) {
	case AV_SAMPLE_FMT_S16:
	case AV_SAMPLE_FMT_S16P:
		return SampleFormat::S16;

	case AV_SAMPLE_FMT_S32:
	case AV_SAMPLE_FMT_S32P:
		return SampleFormat::S32;

	case AV_SAMPLE_FMT_FLTP:
		return SampleFormat::FLOAT;

	default:
		break;
	}

	char buffer[64];
	const char *name = av_get_sample_fmt_string(buffer, sizeof(buffer),
						    sample_fmt);
	if (name != NULL)
		g_warning("Unsupported libavcodec SampleFormat value: %s (%d)",
			  name, sample_fmt);
	else
		g_warning("Unsupported libavcodec SampleFormat value: %d",
			  sample_fmt);
	return SampleFormat::UNDEFINED;
}

static AVInputFormat *
ffmpeg_probe(struct decoder *decoder, struct input_stream *is)
{
	enum {
		BUFFER_SIZE = 16384,
		PADDING = 16,
	};

	unsigned char *buffer = (unsigned char *)g_malloc(BUFFER_SIZE);
	size_t nbytes = decoder_read(decoder, is, buffer, BUFFER_SIZE);
	if (nbytes <= PADDING ||
	    !input_stream_lock_seek(is, 0, SEEK_SET, NULL)) {
		g_free(buffer);
		return NULL;
	}

	/* some ffmpeg parsers (e.g. ac3_parser.c) read a few bytes
	   beyond the declared buffer limit, which makes valgrind
	   angry; this workaround removes some padding from the buffer
	   size */
	nbytes -= PADDING;

	AVProbeData avpd;
	avpd.buf = buffer;
	avpd.buf_size = nbytes;
	avpd.filename = is->uri.c_str();

	AVInputFormat *format = av_probe_input_format(&avpd, true);
	g_free(buffer);

	return format;
}

static void
ffmpeg_decode(struct decoder *decoder, struct input_stream *input)
{
	AVInputFormat *input_format = ffmpeg_probe(decoder, input);
	if (input_format == NULL)
		return;

	g_debug("detected input format '%s' (%s)",
		input_format->name, input_format->long_name);

	AvioStream stream(decoder, input);
	if (!stream.Open()) {
		g_warning("Failed to open stream");
		return;
	}

	//ffmpeg works with ours "fileops" helper
	AVFormatContext *format_context = NULL;
	if (mpd_ffmpeg_open_input(&format_context, stream.io,
				  input->uri.c_str(),
				  input_format) != 0) {
		g_warning("Open failed\n");
		return;
	}

	const int find_result =
		avformat_find_stream_info(format_context, NULL);
	if (find_result < 0) {
		g_warning("Couldn't find stream info\n");
		avformat_close_input(&format_context);
		return;
	}

	int audio_stream = ffmpeg_find_audio_stream(format_context);
	if (audio_stream == -1) {
		g_warning("No audio stream inside\n");
		avformat_close_input(&format_context);
		return;
	}

	AVStream *av_stream = format_context->streams[audio_stream];

	AVCodecContext *codec_context = av_stream->codec;
	if (codec_context->codec_name[0] != 0)
		g_debug("codec '%s'", codec_context->codec_name);

	AVCodec *codec = avcodec_find_decoder(codec_context->codec_id);

	if (!codec) {
		g_warning("Unsupported audio codec\n");
		avformat_close_input(&format_context);
		return;
	}

	const SampleFormat sample_format =
		ffmpeg_sample_format(codec_context->sample_fmt);
	if (sample_format == SampleFormat::UNDEFINED)
		return;

	GError *error = NULL;
	AudioFormat audio_format;
	if (!audio_format_init_checked(audio_format,
				       codec_context->sample_rate,
				       sample_format,
				       codec_context->channels, &error)) {
		g_warning("%s", error->message);
		g_error_free(error);
		avformat_close_input(&format_context);
		return;
	}

	/* the audio format must be read from AVCodecContext by now,
	   because avcodec_open() has been demonstrated to fill bogus
	   values into AVCodecContext.channels - a change that will be
	   reverted later by avcodec_decode_audio3() */

	const int open_result = avcodec_open2(codec_context, codec, NULL);
	if (open_result < 0) {
		g_warning("Could not open codec\n");
		avformat_close_input(&format_context);
		return;
	}

	int total_time = format_context->duration != (int64_t)AV_NOPTS_VALUE
		? format_context->duration / AV_TIME_BASE
		: 0;

	decoder_initialized(decoder, audio_format,
			    input->seekable, total_time);

	AVFrame *frame = avcodec_alloc_frame();
	if (!frame) {
		g_warning("Could not allocate frame\n");
		avformat_close_input(&format_context);
		return;
	}

	enum decoder_command cmd;
	do {
		AVPacket packet;
		if (av_read_frame(format_context, &packet) < 0)
			/* end of file */
			break;

		if (packet.stream_index == audio_stream)
			cmd = ffmpeg_send_packet(decoder, input,
						 &packet, codec_context,
						 &av_stream->time_base,
						 frame);
		else
			cmd = decoder_get_command(decoder);

		av_free_packet(&packet);

		if (cmd == DECODE_COMMAND_SEEK) {
			int64_t where =
				time_to_ffmpeg(decoder_seek_where(decoder),
					       av_stream->time_base);

			if (av_seek_frame(format_context, audio_stream, where,
					  AV_TIME_BASE) < 0)
				decoder_seek_error(decoder);
			else {
				avcodec_flush_buffers(codec_context);
				decoder_command_finished(decoder);
			}
		}
	} while (cmd != DECODE_COMMAND_STOP);

#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(54, 28, 0)
	avcodec_free_frame(&frame);
#else
	av_freep(&frame);
#endif

	avcodec_close(codec_context);
	avformat_close_input(&format_context);
}

//no tag reading in ffmpeg, check if playable
static bool
ffmpeg_scan_stream(struct input_stream *is,
		   const struct tag_handler *handler, void *handler_ctx)
{
	AVInputFormat *input_format = ffmpeg_probe(NULL, is);
	if (input_format == NULL)
		return false;

	AvioStream stream(nullptr, is);
	if (!stream.Open())
		return false;

	AVFormatContext *f = NULL;
	if (mpd_ffmpeg_open_input(&f, stream.io, is->uri.c_str(),
				  input_format) != 0)
		return false;

	const int find_result =
		avformat_find_stream_info(f, NULL);
	if (find_result < 0) {
		avformat_close_input(&f);
		return false;
	}

	if (f->duration != (int64_t)AV_NOPTS_VALUE)
		tag_handler_invoke_duration(handler, handler_ctx,
					    f->duration / AV_TIME_BASE);

	ffmpeg_scan_dictionary(f->metadata, handler, handler_ctx);
	int idx = ffmpeg_find_audio_stream(f);
	if (idx >= 0)
		ffmpeg_scan_dictionary(f->streams[idx]->metadata,
				       handler, handler_ctx);

	avformat_close_input(&f);
	return true;
}

/**
 * A list of extensions found for the formats supported by ffmpeg.
 * This list is current as of 02-23-09; To find out if there are more
 * supported formats, check the ffmpeg changelog since this date for
 * more formats.
 */
static const char *const ffmpeg_suffixes[] = {
	"16sv", "3g2", "3gp", "4xm", "8svx", "aa3", "aac", "ac3", "afc", "aif",
	"aifc", "aiff", "al", "alaw", "amr", "anim", "apc", "ape", "asf",
	"atrac", "au", "aud", "avi", "avm2", "avs", "bap", "bfi", "c93", "cak",
	"cin", "cmv", "cpk", "daud", "dct", "divx", "dts", "dv", "dvd", "dxa",
	"eac3", "film", "flac", "flc", "fli", "fll", "flx", "flv", "g726",
	"gsm", "gxf", "iss", "m1v", "m2v", "m2t", "m2ts",
	"m4a", "m4b", "m4v",
	"mad",
	"mj2", "mjpeg", "mjpg", "mka", "mkv", "mlp", "mm", "mmf", "mov", "mp+",
	"mp1", "mp2", "mp3", "mp4", "mpc", "mpeg", "mpg", "mpga", "mpp", "mpu",
	"mve", "mvi", "mxf", "nc", "nsv", "nut", "nuv", "oga", "ogm", "ogv",
	"ogx", "oma", "ogg", "omg", "psp", "pva", "qcp", "qt", "r3d", "ra",
	"ram", "rl2", "rm", "rmvb", "roq", "rpl", "rvc", "shn", "smk", "snd",
	"sol", "son", "spx", "str", "swf", "tgi", "tgq", "tgv", "thp", "ts",
	"tsp", "tta", "xa", "xvid", "uv", "uv2", "vb", "vid", "vob", "voc",
	"vp6", "vmd", "wav", "webm", "wma", "wmv", "wsaud", "wsvga", "wv",
	"wve",
	NULL
};

static const char *const ffmpeg_mime_types[] = {
	"application/flv",
	"application/m4a",
	"application/mp4",
	"application/octet-stream",
	"application/ogg",
	"application/x-ms-wmz",
	"application/x-ms-wmd",
	"application/x-ogg",
	"application/x-shockwave-flash",
	"application/x-shorten",
	"audio/8svx",
	"audio/16sv",
	"audio/aac",
	"audio/ac3",
	"audio/aiff"
	"audio/amr",
	"audio/basic",
	"audio/flac",
	"audio/m4a",
	"audio/mp4",
	"audio/mpeg",
	"audio/musepack",
	"audio/ogg",
	"audio/qcelp",
	"audio/vorbis",
	"audio/vorbis+ogg",
	"audio/x-8svx",
	"audio/x-16sv",
	"audio/x-aac",
	"audio/x-ac3",
	"audio/x-aiff"
	"audio/x-alaw",
	"audio/x-au",
	"audio/x-dca",
	"audio/x-eac3",
	"audio/x-flac",
	"audio/x-gsm",
	"audio/x-mace",
	"audio/x-matroska",
	"audio/x-monkeys-audio",
	"audio/x-mpeg",
	"audio/x-ms-wma",
	"audio/x-ms-wax",
	"audio/x-musepack",
	"audio/x-ogg",
	"audio/x-vorbis",
	"audio/x-vorbis+ogg",
	"audio/x-pn-realaudio",
	"audio/x-pn-multirate-realaudio",
	"audio/x-speex",
	"audio/x-tta"
	"audio/x-voc",
	"audio/x-wav",
	"audio/x-wma",
	"audio/x-wv",
	"video/anim",
	"video/quicktime",
	"video/msvideo",
	"video/ogg",
	"video/theora",
	"video/webm",
	"video/x-dv",
	"video/x-flv",
	"video/x-matroska",
	"video/x-mjpeg",
	"video/x-mpeg",
	"video/x-ms-asf",
	"video/x-msvideo",
	"video/x-ms-wmv",
	"video/x-ms-wvx",
	"video/x-ms-wm",
	"video/x-ms-wmx",
	"video/x-nut",
	"video/x-pva",
	"video/x-theora",
	"video/x-vid",
	"video/x-wmv",
	"video/x-xvid",

	/* special value for the "ffmpeg" input plugin: all streams by
	   the "ffmpeg" input plugin shall be decoded by this
	   plugin */
	"audio/x-mpd-ffmpeg",

	NULL
};

const struct decoder_plugin ffmpeg_decoder_plugin = {
	"ffmpeg",
	ffmpeg_init,
	nullptr,
	ffmpeg_decode,
	nullptr,
	nullptr,
	ffmpeg_scan_stream,
	nullptr,
	ffmpeg_suffixes,
	ffmpeg_mime_types
};
