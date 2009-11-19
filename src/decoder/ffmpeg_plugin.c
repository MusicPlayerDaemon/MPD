/*
 * Copyright (C) 2003-2009 The Music Player Daemon Project
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

#include <glib.h>

#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#ifdef OLD_FFMPEG_INCLUDES
#include <avcodec.h>
#include <avformat.h>
#include <avio.h>
#else
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavformat/avio.h>
#endif

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "ffmpeg"

struct ffmpeg_context {
	int audio_stream;
	AVFormatContext *format_context;
	AVCodecContext *codec_context;
	struct decoder *decoder;
	struct input_stream *input;
	struct tag *tag;
};

struct ffmpeg_stream {
	/** hack - see url_to_struct() */
	char url[8];

	struct decoder *decoder;
	struct input_stream *input;
};

/**
 * Convert a faked mpd:// URL to a ffmpeg_stream structure.  This is a
 * hack because ffmpeg does not provide a nice API for passing a
 * user-defined pointer to mpdurl_open().
 */
static struct ffmpeg_stream *url_to_struct(const char *url)
{
	union {
		const char *in;
		struct ffmpeg_stream *out;
	} u = { .in = url };
	return u.out;
}

static int mpd_ffmpeg_open(URLContext *h, const char *filename,
			   G_GNUC_UNUSED int flags)
{
	struct ffmpeg_stream *stream = url_to_struct(filename);
	h->priv_data = stream;
	h->is_streamed = stream->input->seekable ? 0 : 1;
	return 0;
}

static int mpd_ffmpeg_read(URLContext *h, unsigned char *buf, int size)
{
	struct ffmpeg_stream *stream = (struct ffmpeg_stream *) h->priv_data;

	return decoder_read(stream->decoder, stream->input,
			    (void *)buf, size);
}

static int64_t mpd_ffmpeg_seek(URLContext *h, int64_t pos, int whence)
{
	struct ffmpeg_stream *stream = (struct ffmpeg_stream *) h->priv_data;
	bool ret;

	if (whence == AVSEEK_SIZE)
		return stream->input->size;

	ret = input_stream_seek(stream->input, pos, whence);
	if (!ret)
		return -1;

	return stream->input->offset;
}

static int mpd_ffmpeg_close(URLContext *h)
{
	h->priv_data = NULL;
	return 0;
}

static URLProtocol mpd_ffmpeg_fileops = {
	.name = "mpd",
	.url_open = mpd_ffmpeg_open,
	.url_read = mpd_ffmpeg_read,
	.url_seek = mpd_ffmpeg_seek,
	.url_close = mpd_ffmpeg_close,
};

static bool
ffmpeg_init(G_GNUC_UNUSED const struct config_param *param)
{
	av_register_all();
	register_protocol(&mpd_ffmpeg_fileops);
	return true;
}

static int
ffmpeg_find_audio_stream(const AVFormatContext *format_context)
{
	for (unsigned i = 0; i < format_context->nb_streams; ++i)
		if (format_context->streams[i]->codec->codec_type ==
		    CODEC_TYPE_AUDIO)
			return i;

	return -1;
}

static bool
ffmpeg_helper(struct input_stream *input,
	      bool (*callback)(struct ffmpeg_context *ctx),
	      struct ffmpeg_context *ctx)
{
	AVFormatContext *format_context;
	AVCodecContext *codec_context;
	AVCodec *codec;
	int audio_stream;
	struct ffmpeg_stream stream = {
		.url = "mpd://X", /* only the mpd:// prefix matters */
	};
	bool ret;

	stream.input = input;
	if (ctx && ctx->decoder) {
		stream.decoder = ctx->decoder; //are we in decoding loop ?
	} else {
		stream.decoder = NULL;
	}

	//ffmpeg works with ours "fileops" helper
	if (av_open_input_file(&format_context, stream.url, NULL, 0, NULL) != 0) {
		g_warning("Open failed\n");
		return false;
	}

	if (av_find_stream_info(format_context)<0) {
		g_warning("Couldn't find stream info\n");
		return false;
	}

	audio_stream = ffmpeg_find_audio_stream(format_context);
	if (audio_stream == -1) {
		g_warning("No audio stream inside\n");
		return false;
	}

	codec_context = format_context->streams[audio_stream]->codec;
	if (codec_context->codec_name[0] != 0)
		g_debug("codec '%s'", codec_context->codec_name);

	codec = avcodec_find_decoder(codec_context->codec_id);

	if (!codec) {
		g_warning("Unsupported audio codec\n");
		return false;
	}

	if (avcodec_open(codec_context, codec)<0) {
		g_warning("Could not open codec\n");
		return false;
	}

	if (callback) {
		ctx->audio_stream = audio_stream;
		ctx->format_context = format_context;
		ctx->codec_context = codec_context;

		ret = callback(ctx);
	} else
		ret = true;

	avcodec_close(codec_context);
	av_close_input_file(format_context);

	return ret;
}

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

static enum decoder_command
ffmpeg_send_packet(struct decoder *decoder, struct input_stream *is,
		   const AVPacket *packet,
		   AVCodecContext *codec_context,
		   const AVRational *time_base)
{
	enum decoder_command cmd = DECODE_COMMAND_NONE;
	int position;
	uint8_t audio_buf[(AVCODEC_MAX_AUDIO_FRAME_SIZE * 3) / 2 + 16];
	int16_t *aligned_buffer;
	size_t buffer_size;
	int len, audio_size;
	uint8_t *packet_data;
	int packet_size;

	packet_data = packet->data;
	packet_size = packet->size;

	buffer_size = sizeof(audio_buf);
	aligned_buffer = align16(audio_buf, &buffer_size);

	while ((packet_size > 0) && (cmd == DECODE_COMMAND_NONE)) {
		audio_size = buffer_size;
		len = avcodec_decode_audio2(codec_context,
					    aligned_buffer, &audio_size,
					    packet_data, packet_size);

		if (len < 0) {
			/* if error, we skip the frame */
			g_message("decoding failed\n");
			break;
		}

		packet_data += len;
		packet_size -= len;

		if (audio_size <= 0)
			continue;

		position = packet->pts != (int64_t)AV_NOPTS_VALUE
			? av_rescale_q(packet->pts, *time_base,
				       (AVRational){1, 1})
			: 0;

		cmd = decoder_data(decoder, is,
				   aligned_buffer, audio_size,
				   position,
				   codec_context->bit_rate / 1000, NULL);
	}
	return cmd;
}

static bool
ffmpeg_decode_internal(struct ffmpeg_context *ctx)
{
	GError *error = NULL;
	struct decoder *decoder = ctx->decoder;
	AVCodecContext *codec_context = ctx->codec_context;
	AVFormatContext *format_context = ctx->format_context;
	AVPacket packet;
	struct audio_format audio_format;
	enum decoder_command cmd;
	int total_time;
	uint8_t bits;

	total_time = 0;

	if (codec_context->channels > 2) {
		codec_context->channels = 2;
	}

#if LIBAVCODEC_VERSION_INT >= ((51<<16)+(41<<8)+0)
	bits = (uint8_t) av_get_bits_per_sample_format(codec_context->sample_fmt);
#else
	/* XXX fixme 16-bit for older ffmpeg (13 Aug 2007) */
	bits = (uint8_t) 16;
#endif
	if (!audio_format_init_checked(&audio_format,
				       codec_context->sample_rate, bits,
				       codec_context->channels, &error)) {
		g_warning("%s", error->message);
		g_error_free(error);
		return false;
	}

	//there is some problem with this on some demux (mp3 at least)
	if (format_context->duration != (int64_t)AV_NOPTS_VALUE) {
		total_time = format_context->duration / AV_TIME_BASE;
	}

	decoder_initialized(decoder, &audio_format,
			    ctx->input->seekable, total_time);

	do {
		if (av_read_frame(format_context, &packet) < 0)
			/* end of file */
			break;

		if (packet.stream_index == ctx->audio_stream)
			cmd = ffmpeg_send_packet(decoder, ctx->input,
						 &packet, codec_context,
						 &format_context->streams[ctx->audio_stream]->time_base);
		else
			cmd = decoder_get_command(decoder);

		av_free_packet(&packet);

		if (cmd == DECODE_COMMAND_SEEK) {
			int64_t where =
				decoder_seek_where(decoder) * AV_TIME_BASE;

			if (av_seek_frame(format_context, -1, where, 0) < 0)
				decoder_seek_error(decoder);
			else
				decoder_command_finished(decoder);
		}
	} while (cmd != DECODE_COMMAND_STOP);

	return true;
}

static void
ffmpeg_decode(struct decoder *decoder, struct input_stream *input)
{
	struct ffmpeg_context ctx;

	ctx.input = input;
	ctx.decoder = decoder;

	ffmpeg_helper(input, ffmpeg_decode_internal, &ctx);
}

#if LIBAVFORMAT_VERSION_INT >= ((52<<16)+(31<<8)+0)
static bool
ffmpeg_copy_metadata(struct tag *tag, AVMetadata *m,
		     enum tag_type type, const char *name)
{
	AVMetadataTag *mt = NULL;

	while ((mt = av_metadata_get(m, name, mt, 0)) != NULL)
		tag_add_item(tag, type, mt->value);
	return mt != NULL;
}
#endif

static bool ffmpeg_tag_internal(struct ffmpeg_context *ctx)
{
	struct tag *tag = (struct tag *) ctx->tag;
	AVFormatContext *f = ctx->format_context;

	tag->time = 0;
	if (f->duration != (int64_t)AV_NOPTS_VALUE)
		tag->time = f->duration / AV_TIME_BASE;

#if LIBAVFORMAT_VERSION_INT >= ((52<<16)+(31<<8)+0)
	av_metadata_conv(f, NULL, f->iformat->metadata_conv);

	ffmpeg_copy_metadata(tag, f->metadata, TAG_TITLE, "title");
	ffmpeg_copy_metadata(tag, f->metadata, TAG_ARTIST, "author");
	ffmpeg_copy_metadata(tag, f->metadata, TAG_ALBUM, "album");
	ffmpeg_copy_metadata(tag, f->metadata, TAG_COMMENT, "comment");
	ffmpeg_copy_metadata(tag, f->metadata, TAG_GENRE, "genre");
	ffmpeg_copy_metadata(tag, f->metadata, TAG_TRACK, "track");
	ffmpeg_copy_metadata(tag, f->metadata, TAG_DATE, "year");
#else
	if (f->author[0])
		tag_add_item(tag, TAG_ARTIST, f->author);
	if (f->title[0])
		tag_add_item(tag, TAG_TITLE, f->title);
	if (f->album[0])
		tag_add_item(tag, TAG_ALBUM, f->album);

	if (f->track > 0) {
		char buffer[16];
		snprintf(buffer, sizeof(buffer), "%d", f->track);
		tag_add_item(tag, TAG_TRACK, buffer);
	}

	if (f->comment[0])
		tag_add_item(tag, TAG_COMMENT, f->comment);
	if (f->genre[0])
		tag_add_item(tag, TAG_GENRE, f->genre);
	if (f->year > 0) {
		char buffer[16];
		snprintf(buffer, sizeof(buffer), "%d", f->year);
		tag_add_item(tag, TAG_DATE, buffer);
	}

#endif
	return true;
}

//no tag reading in ffmpeg, check if playable
static struct tag *ffmpeg_tag(const char *file)
{
	struct input_stream input;
	struct ffmpeg_context ctx;
	bool ret;

	if (!input_stream_open(&input, file)) {
		g_warning("failed to open %s\n", file);
		return NULL;
	}

	ctx.decoder = NULL;
	ctx.tag = tag_new();

	ret = ffmpeg_helper(&input, ffmpeg_tag_internal, &ctx);
	if (!ret) {
		tag_free(ctx.tag);
		ctx.tag = NULL;
	}

	input_stream_close(&input);

	return ctx.tag;
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
	"gsm", "gxf", "iss", "m1v", "m2v", "m2t", "m2ts", "m4a", "m4v", "mad",
	"mj2", "mjpeg", "mjpg", "mka", "mkv", "mlp", "mm", "mmf", "mov", "mp+",
	"mp1", "mp2", "mp3", "mp4", "mpc", "mpeg", "mpg", "mpga", "mpp", "mpu",
	"mve", "mvi", "mxf", "nc", "nsv", "nut", "nuv", "oga", "ogm", "ogv",
	"ogx", "oma", "ogg", "omg", "psp", "pva", "qcp", "qt", "r3d", "ra",
	"ram", "rl2", "rm", "rmvb", "roq", "rpl", "rvc", "shn", "smk", "snd",
	"sol", "son", "spx", "str", "swf", "tgi", "tgq", "tgv", "thp", "ts",
	"tsp", "tta", "xa", "xvid", "uv", "uv2", "vb", "vid", "vob", "voc",
	"vp6", "vmd", "wav", "wma", "wmv", "wsaud", "wsvga", "wv", "wve",
	NULL
};

static const char *const ffmpeg_mime_types[] = {
	"application/mp4",
	"application/octet-stream",
	"application/ogg",
	"application/x-ms-wmz",
	"application/x-ms-wmd",
	"application/x-shockwave-flash",
	"application/x-shorten",
	"audio/8svx",
	"audio/16sv",
	"audio/aac",
	"audio/ac3",
	"audio/amr",
	"audio/basic",
	"audio/flac",
	"audio/mpeg",
	"audio/musepack",
	"audio/ogg",
	"audio/qcelp",
	"audio/vorbis",
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
	"audio/x-monkeys-audio",
	"audio/x-mpeg",
	"audio/x-ms-wma",
	"audio/x-ms-wax",
	"audio/x-musepack",
	"audio/x-pn-realaudio",
	"audio/x-pn-multirate-realaudio",
	"audio/x-speex",
	"audio/x-tta"
	"audio/x-wav",
	"audio/x-wma",
	"audio/x-wv",
	"video/anim",
	"video/quicktime",
	"video/msvideo",
	"video/ogg",
	"video/theora",
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
	NULL
};

const struct decoder_plugin ffmpeg_decoder_plugin = {
	.name = "ffmpeg",
	.init = ffmpeg_init,
	.stream_decode = ffmpeg_decode,
	.tag_dup = ffmpeg_tag,
	.suffixes = ffmpeg_suffixes,
	.mime_types = ffmpeg_mime_types
};
