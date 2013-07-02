/*
 * Copyright (C) 2003-2011 The Music Player Daemon Project
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
#include "decoder_api.h"
#include "audio_check.h"
#include "ffmpeg_metadata.h"
#include "tag_handler.h"

#include <glib.h>

#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavformat/avio.h>
#include <libavutil/avutil.h>
#include <libavutil/log.h>
#include <libavutil/mathematics.h>
#if LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(51,5,0)
#include <libavutil/dict.h>
#endif

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "ffmpeg"

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
mpd_ffmpeg_log_callback(G_GNUC_UNUSED void *ptr, int level,
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

struct mpd_ffmpeg_stream {
	struct decoder *decoder;
	struct input_stream *input;

#if LIBAVFORMAT_VERSION_INT >= AV_VERSION_INT(52,101,0)
	AVIOContext *io;
#else
	ByteIOContext *io;
#endif
	unsigned char buffer[8192];
};

static int
mpd_ffmpeg_stream_read(void *opaque, uint8_t *buf, int size)
{
	struct mpd_ffmpeg_stream *stream = opaque;

	return decoder_read(stream->decoder, stream->input,
			    (void *)buf, size);
}

static int64_t
mpd_ffmpeg_stream_seek(void *opaque, int64_t pos, int whence)
{
	struct mpd_ffmpeg_stream *stream = opaque;

	if (whence == AVSEEK_SIZE)
		return stream->input->size;

	if (!input_stream_lock_seek(stream->input, pos, whence, NULL))
		return -1;

	return stream->input->offset;
}

static struct mpd_ffmpeg_stream *
mpd_ffmpeg_stream_open(struct decoder *decoder, struct input_stream *input)
{
	struct mpd_ffmpeg_stream *stream = g_new(struct mpd_ffmpeg_stream, 1);
	stream->decoder = decoder;
	stream->input = input;
#if LIBAVFORMAT_VERSION_INT >= AV_VERSION_INT(52,101,0)
	stream->io = avio_alloc_context(stream->buffer, sizeof(stream->buffer),
					false, stream,
					mpd_ffmpeg_stream_read, NULL,
					input->seekable
					? mpd_ffmpeg_stream_seek : NULL);
#else
	stream->io = av_alloc_put_byte(stream->buffer, sizeof(stream->buffer),
				       false, stream,
				       mpd_ffmpeg_stream_read, NULL,
				       input->seekable
				       ? mpd_ffmpeg_stream_seek : NULL);
#endif
	if (stream->io == NULL) {
		g_free(stream);
		return NULL;
	}

	return stream;
}

/**
 * API compatibility wrapper for av_open_input_stream() and
 * avformat_open_input().
 */
static int
mpd_ffmpeg_open_input(AVFormatContext **ic_ptr,
#if LIBAVFORMAT_VERSION_INT >= AV_VERSION_INT(52,101,0)
		      AVIOContext *pb,
#else
		      ByteIOContext *pb,
#endif
		      const char *filename,
		      AVInputFormat *fmt)
{
#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(53,1,3)
	AVFormatContext *context = avformat_alloc_context();
	if (context == NULL)
		return AVERROR(ENOMEM);

	context->pb = pb;
	*ic_ptr = context;
	return avformat_open_input(ic_ptr, filename, fmt, NULL);
#else
	return av_open_input_stream(ic_ptr, pb, filename, fmt, NULL);
#endif
}

static void
mpd_ffmpeg_stream_close(struct mpd_ffmpeg_stream *stream)
{
	av_free(stream->io);
	g_free(stream);
}

static bool
ffmpeg_init(G_GNUC_UNUSED const struct config_param *param)
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
#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(52, 64, 0)
		    AVMEDIA_TYPE_AUDIO)
#else
		    CODEC_TYPE_AUDIO)
#endif
			return i;

	return -1;
}

#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(53,25,0)
/**
 * On some platforms, libavcodec wants the output buffer aligned to 16
 * bytes (because it uses SSE/Altivec internally).  This function
 * returns the aligned version of the specified buffer, and corrects
 * the buffer size.
 */
static void *
align16(void *p, size_t *length_p)
{
	unsigned add = 16 - (size_t)p % 16;

	*length_p -= add;
	return (char *)p + add;
}
#endif

G_GNUC_CONST
static double
time_from_ffmpeg(int64_t t, const AVRational time_base)
{
	assert(t != (int64_t)AV_NOPTS_VALUE);

	return (double)av_rescale_q(t, time_base, (AVRational){1, 1024})
		/ (double)1024;
}

G_GNUC_CONST
static int64_t
time_to_ffmpeg(double t, const AVRational time_base)
{
	return av_rescale_q((int64_t)(t * 1024), (AVRational){1, 1024},
			    time_base);
}

#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(53,25,0)

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
#endif

static enum decoder_command
ffmpeg_send_packet(struct decoder *decoder, struct input_stream *is,
		   const AVPacket *packet,
		   AVCodecContext *codec_context,
		   const AVRational *time_base)
{
	if (packet->pts >= 0 && packet->pts != (int64_t)AV_NOPTS_VALUE)
		decoder_timestamp(decoder,
				  time_from_ffmpeg(packet->pts, *time_base));

#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(52,25,0)
	AVPacket packet2 = *packet;
#else
	const uint8_t *packet_data = packet->data;
	int packet_size = packet->size;
#endif

#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(53,25,0)
	uint8_t aligned_buffer[(AVCODEC_MAX_AUDIO_FRAME_SIZE * 3) / 2 + 16];
	const size_t buffer_size = sizeof(aligned_buffer);
#else
	/* libavcodec < 0.8 needs an aligned buffer */
	uint8_t audio_buf[(AVCODEC_MAX_AUDIO_FRAME_SIZE * 3) / 2 + 16];
	size_t buffer_size = sizeof(audio_buf);
	int16_t *aligned_buffer = align16(audio_buf, &buffer_size);
#endif

	enum decoder_command cmd = DECODE_COMMAND_NONE;
	while (
#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(52,25,0)
	       packet2.size > 0 &&
#else
	       packet_size > 0 &&
#endif
	       cmd == DECODE_COMMAND_NONE) {
		int audio_size = buffer_size;
#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(53,25,0)

		AVFrame *frame = avcodec_alloc_frame();
		if (frame == NULL) {
			g_warning("Could not allocate frame");
			break;
		}

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

#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(54, 28, 0)
		avcodec_free_frame(&frame);
#else
		av_freep(&frame);
#endif

#elif LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(52,25,0)
		int len = avcodec_decode_audio3(codec_context,
						aligned_buffer, &audio_size,
						&packet2);
#else
		int len = avcodec_decode_audio2(codec_context,
						aligned_buffer, &audio_size,
						packet_data, packet_size);
#endif

		if (len < 0) {
			/* if error, we skip the frame */
			g_message("decoding failed, frame skipped\n");
			break;
		}

#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(52,25,0)
		packet2.data += len;
		packet2.size -= len;
#else
		packet_data += len;
		packet_size -= len;
#endif

		if (audio_size <= 0)
			continue;

		cmd = decoder_data(decoder, is,
				   aligned_buffer, audio_size,
				   codec_context->bit_rate / 1000);
	}
	return cmd;
}

#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(52, 94, 1)
#define AVSampleFormat SampleFormat
#endif

G_GNUC_CONST
static enum sample_format
ffmpeg_sample_format(enum AVSampleFormat sample_fmt)
{
	switch (sample_fmt) {
#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(52, 94, 1)
	case AV_SAMPLE_FMT_S16:
#if LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(51,17,0)
	case AV_SAMPLE_FMT_S16P:
#endif
#else
	case SAMPLE_FMT_S16:
#endif
		return SAMPLE_FORMAT_S16;

#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(52, 94, 1)
	case AV_SAMPLE_FMT_S32:
#if LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(51,17,0)
	case AV_SAMPLE_FMT_S32P:
#endif
#else
	case SAMPLE_FMT_S32:
#endif
		return SAMPLE_FORMAT_S32;

#if LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(51,17,0)
	case AV_SAMPLE_FMT_FLTP:
		return SAMPLE_FORMAT_FLOAT;
#endif

	default:
		break;
	}

#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(52, 94, 1)
	char buffer[64];
	const char *name = av_get_sample_fmt_string(buffer, sizeof(buffer),
						    sample_fmt);
	if (name != NULL)
		g_warning("Unsupported libavcodec SampleFormat value: %s (%d)",
			  name, sample_fmt);
	else
#endif
		g_warning("Unsupported libavcodec SampleFormat value: %d",
			  sample_fmt);
	return SAMPLE_FORMAT_UNDEFINED;
}

static AVInputFormat *
ffmpeg_probe(struct decoder *decoder, struct input_stream *is)
{
	enum {
		BUFFER_SIZE = 16384,
		PADDING = 16,
	};

	unsigned char *buffer = g_malloc(BUFFER_SIZE);
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

	AVProbeData avpd = {
		.buf = buffer,
		.buf_size = nbytes,
		.filename = is->uri,
	};

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

	struct mpd_ffmpeg_stream *stream =
		mpd_ffmpeg_stream_open(decoder, input);
	if (stream == NULL) {
		g_warning("Failed to open stream");
		return;
	}

	//ffmpeg works with ours "fileops" helper
	AVFormatContext *format_context = NULL;
	if (mpd_ffmpeg_open_input(&format_context, stream->io, input->uri,
				  input_format) != 0) {
		g_warning("Open failed\n");
		mpd_ffmpeg_stream_close(stream);
		return;
	}

#if LIBAVFORMAT_VERSION_INT >= AV_VERSION_INT(53,6,0)
	const int find_result =
		avformat_find_stream_info(format_context, NULL);
#else
	const int find_result = av_find_stream_info(format_context);
#endif
	if (find_result < 0) {
		g_warning("Couldn't find stream info\n");
#if LIBAVFORMAT_VERSION_INT >= AV_VERSION_INT(53,17,0)
		avformat_close_input(&format_context);
#else
		av_close_input_stream(format_context);
#endif
		mpd_ffmpeg_stream_close(stream);
		return;
	}

	int audio_stream = ffmpeg_find_audio_stream(format_context);
	if (audio_stream == -1) {
		g_warning("No audio stream inside\n");
#if LIBAVFORMAT_VERSION_INT >= AV_VERSION_INT(53,17,0)
		avformat_close_input(&format_context);
#else
		av_close_input_stream(format_context);
#endif
		mpd_ffmpeg_stream_close(stream);
		return;
	}

	AVStream *av_stream = format_context->streams[audio_stream];

	AVCodecContext *codec_context = av_stream->codec;
	if (codec_context->codec_name[0] != 0)
		g_debug("codec '%s'", codec_context->codec_name);

	AVCodec *codec = avcodec_find_decoder(codec_context->codec_id);

	if (!codec) {
		g_warning("Unsupported audio codec\n");
#if LIBAVFORMAT_VERSION_INT >= AV_VERSION_INT(53,17,0)
		avformat_close_input(&format_context);
#else
		av_close_input_stream(format_context);
#endif
		mpd_ffmpeg_stream_close(stream);
		return;
	}

	const enum sample_format sample_format =
		ffmpeg_sample_format(codec_context->sample_fmt);
	if (sample_format == SAMPLE_FORMAT_UNDEFINED)
		return;

	GError *error = NULL;
	struct audio_format audio_format;
	if (!audio_format_init_checked(&audio_format,
				       codec_context->sample_rate,
				       sample_format,
				       codec_context->channels, &error)) {
		g_warning("%s", error->message);
		g_error_free(error);
#if LIBAVFORMAT_VERSION_INT >= AV_VERSION_INT(53,17,0)
		avformat_close_input(&format_context);
#else
		av_close_input_stream(format_context);
#endif
		mpd_ffmpeg_stream_close(stream);
		return;
	}

	/* the audio format must be read from AVCodecContext by now,
	   because avcodec_open() has been demonstrated to fill bogus
	   values into AVCodecContext.channels - a change that will be
	   reverted later by avcodec_decode_audio3() */

#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(53,6,0)
	const int open_result = avcodec_open2(codec_context, codec, NULL);
#else
	const int open_result = avcodec_open(codec_context, codec);
#endif
	if (open_result < 0) {
		g_warning("Could not open codec\n");
#if LIBAVFORMAT_VERSION_INT >= AV_VERSION_INT(53,17,0)
		avformat_close_input(&format_context);
#else
		av_close_input_stream(format_context);
#endif
		mpd_ffmpeg_stream_close(stream);
		return;
	}

	int total_time = format_context->duration != (int64_t)AV_NOPTS_VALUE
		? format_context->duration / AV_TIME_BASE
		: 0;

	decoder_initialized(decoder, &audio_format,
			    input->seekable, total_time);

	enum decoder_command cmd;
	do {
		AVPacket packet;
		if (av_read_frame(format_context, &packet) < 0)
			/* end of file */
			break;

		if (packet.stream_index == audio_stream)
			cmd = ffmpeg_send_packet(decoder, input,
						 &packet, codec_context,
						 &av_stream->time_base);
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

	avcodec_close(codec_context);
#if LIBAVFORMAT_VERSION_INT >= AV_VERSION_INT(53,17,0)
	avformat_close_input(&format_context);
#else
	av_close_input_stream(format_context);
#endif
	mpd_ffmpeg_stream_close(stream);
}

//no tag reading in ffmpeg, check if playable
static bool
ffmpeg_scan_stream(struct input_stream *is,
		   const struct tag_handler *handler, void *handler_ctx)
{
	AVInputFormat *input_format = ffmpeg_probe(NULL, is);
	if (input_format == NULL)
		return false;

	struct mpd_ffmpeg_stream *stream = mpd_ffmpeg_stream_open(NULL, is);
	if (stream == NULL)
		return false;

	AVFormatContext *f = NULL;
	if (mpd_ffmpeg_open_input(&f, stream->io, is->uri,
				  input_format) != 0) {
		mpd_ffmpeg_stream_close(stream);
		return false;
	}

#if LIBAVFORMAT_VERSION_INT >= AV_VERSION_INT(53,6,0)
	const int find_result =
		avformat_find_stream_info(f, NULL);
#else
	const int find_result = av_find_stream_info(f);
#endif
	if (find_result < 0) {
#if LIBAVFORMAT_VERSION_INT >= AV_VERSION_INT(53,17,0)
		avformat_close_input(&f);
#else
		av_close_input_stream(f);
#endif
		mpd_ffmpeg_stream_close(stream);
		return false;
	}

	if (f->duration != (int64_t)AV_NOPTS_VALUE)
		tag_handler_invoke_duration(handler, handler_ctx,
					    f->duration / AV_TIME_BASE);

#if LIBAVFORMAT_VERSION_INT < AV_VERSION_INT(52,101,0)
	av_metadata_conv(f, NULL, f->iformat->metadata_conv);
#endif

	ffmpeg_scan_dictionary(f->metadata, handler, handler_ctx);
	int idx = ffmpeg_find_audio_stream(f);
	if (idx >= 0)
		ffmpeg_scan_dictionary(f->streams[idx]->metadata,
				       handler, handler_ctx);

#if LIBAVFORMAT_VERSION_INT >= AV_VERSION_INT(53,17,0)
	avformat_close_input(&f);
#else
	av_close_input_stream(f);
#endif
	mpd_ffmpeg_stream_close(stream);

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
	.name = "ffmpeg",
	.init = ffmpeg_init,
	.stream_decode = ffmpeg_decode,
	.scan_stream = ffmpeg_scan_stream,
	.suffixes = ffmpeg_suffixes,
	.mime_types = ffmpeg_mime_types
};
