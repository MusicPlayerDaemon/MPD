/* the Music Player Daemon (MPD)
 * Copyright (C) 2008 Viliam Mateicka <viliam.mateicka@gmail.com>
 * This project's homepage is: http://www.musicpd.org
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "../decoder_api.h"
#include "../log.h"
#include "../utils.h"
#include "../log.h"

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

typedef struct {
	int audioStream;
	AVFormatContext *pFormatCtx;
	AVCodecContext *aCodecCtx;
	AVCodec *aCodec;
	struct decoder *decoder;
	struct input_stream *input;
	struct tag *tag;
} BasePtrs;

typedef struct {
	/** hack - see url_to_base() */
	char url[8];

	struct decoder *decoder;
	struct input_stream *input;
} FopsHelper;

/**
 * Convert a faked mpd:// URL to a FopsHelper structure.  This is a
 * hack because ffmpeg does not provide a nice API for passing a
 * user-defined pointer to mpdurl_open().
 */
static FopsHelper *url_to_base(const char *url)
{
	union {
		const char *in;
		FopsHelper *out;
	} u = { .in = url };
	return u.out;
}

static int mpdurl_open(URLContext *h, const char *filename,
		       mpd_unused int flags)
{
	FopsHelper *base = url_to_base(filename);
	h->priv_data = base;
	h->is_streamed = (base->input->seekable ? 0 : 1);
	return 0;
}

static int mpdurl_read(URLContext *h, unsigned char *buf, int size)
{
	FopsHelper *base = (FopsHelper *) h->priv_data;

	while (true) {
		size_t ret = decoder_read(base->decoder, base->input,
					  (void *)buf, size);
		if (ret > 0)
			return ret;

		if (input_stream_eof(base->input) ||
		    (base->decoder &&
		     decoder_get_command(base->decoder) != DECODE_COMMAND_NONE))
			return 0;

		my_usleep(10000);
	}
}

static int64_t mpdurl_seek(URLContext *h, int64_t pos, int whence)
{
	FopsHelper *base = (FopsHelper *) h->priv_data;
	if (whence != AVSEEK_SIZE) { //only ftell
		(void) input_stream_seek(base->input, pos, whence);
	}
	return base->input->offset;
}

static int mpdurl_close(URLContext *h)
{
	FopsHelper *base = (FopsHelper *) h->priv_data;
	if (base && base->input->seekable) {
		(void) input_stream_seek(base->input, 0, SEEK_SET);
	}
	h->priv_data = 0;
	return 0;
}

static URLProtocol mpdurl_fileops = {
	.name = "mpd",
	.url_open = mpdurl_open,
	.url_read = mpdurl_read,
	.url_seek = mpdurl_seek,
	.url_close = mpdurl_close,
};

static bool ffmpeg_init(void)
{
	av_register_all();
	register_protocol(&mpdurl_fileops);
	return true;
}

static bool
ffmpeg_helper(struct input_stream *input, bool (*callback)(BasePtrs *ptrs),
	      BasePtrs *ptrs)
{
	AVFormatContext *pFormatCtx;
	AVCodecContext *aCodecCtx;
	AVCodec *aCodec;
	int audioStream;
	unsigned i;
	FopsHelper fopshelp = {
		.url = "mpd://X", /* only the mpd:// prefix matters */
	};
	bool ret;

	fopshelp.input = input;
	if (ptrs && ptrs->decoder) {
		fopshelp.decoder = ptrs->decoder; //are we in decoding loop ?
	} else {
		fopshelp.decoder = NULL;
	}

	//ffmpeg works with ours "fileops" helper
	if (av_open_input_file(&pFormatCtx, fopshelp.url, NULL, 0, NULL)!=0) {
		ERROR("Open failed!\n");
		return false;
	}

	if (av_find_stream_info(pFormatCtx)<0) {
		ERROR("Couldn't find stream info!\n");
		return false;
	}

	audioStream = -1;
	for(i=0; i<pFormatCtx->nb_streams; i++) {
		if (pFormatCtx->streams[i]->codec->codec_type==CODEC_TYPE_AUDIO &&
		    audioStream < 0) {
			audioStream=i;
		}
	}

	if(audioStream==-1) {
		ERROR("No audio stream inside!\n");
		return false;
	}

	aCodecCtx = pFormatCtx->streams[audioStream]->codec;
	aCodec = avcodec_find_decoder(aCodecCtx->codec_id);

	if (!aCodec) {
		ERROR("Unsupported audio codec!\n");
		return false;
	}

	if (avcodec_open(aCodecCtx, aCodec)<0) {
		ERROR("Could not open codec!\n");
		return false;
	}

	if (callback) {
		ptrs->audioStream = audioStream;
		ptrs->pFormatCtx = pFormatCtx;
		ptrs->aCodecCtx = aCodecCtx;
		ptrs->aCodec = aCodec;

		ret = (*callback)( ptrs );
	} else
		ret = true;

	avcodec_close(aCodecCtx);
	av_close_input_file(pFormatCtx);

	return ret;
}

static bool
ffmpeg_try_decode(struct input_stream *input)
{
	return ffmpeg_helper(input, NULL, NULL);
}

static enum decoder_command
ffmpeg_send_packet(struct decoder *decoder, struct input_stream *is,
		   const AVPacket *packet,
		   AVCodecContext *codec_context,
		   const AVRational *time_base)
{
	int position;
	uint8_t audio_buf[(AVCODEC_MAX_AUDIO_FRAME_SIZE * 3) / 2];
	int len, audio_size;

	position = av_rescale_q(packet->pts, *time_base,
				(AVRational){1, 1});

	audio_size = sizeof(audio_buf);
	len = avcodec_decode_audio2(codec_context,
				    (int16_t *)audio_buf,
				    &audio_size,
				    packet->data, packet->size);

	if (len < 0) {
		WARNING("skipping frame!\n");
		return decoder_get_command(decoder);
	}

	assert(audio_size >= 0);

	return decoder_data(decoder, is,
			    audio_buf, audio_size,
			    position,
			    codec_context->bit_rate / 1000, NULL);
}

static bool
ffmpeg_decode_internal(BasePtrs *base)
{
	struct decoder *decoder = base->decoder;
	AVCodecContext *aCodecCtx = base->aCodecCtx;
	AVFormatContext *pFormatCtx = base->pFormatCtx;
	AVPacket packet;
	struct audio_format audio_format;
	enum decoder_command cmd;
	int current, total_time;

	total_time = 0;

	if (aCodecCtx->channels > 2) {
		aCodecCtx->channels = 2;
	}

	audio_format.bits = (uint8_t)16;
	audio_format.sample_rate = (unsigned int)aCodecCtx->sample_rate;
	audio_format.channels = aCodecCtx->channels;

	//there is some problem with this on some demux (mp3 at least)
	if (pFormatCtx->duration != (int)AV_NOPTS_VALUE) {
		total_time = pFormatCtx->duration / AV_TIME_BASE;
	}

	decoder_initialized(decoder, &audio_format,
			    base->input->seekable, total_time);

	do {
		if (av_read_frame(pFormatCtx, &packet) < 0)
			/* end of file */
			break;

		if (packet.stream_index == base->audioStream)
			cmd = ffmpeg_send_packet(decoder, base->input,
						 &packet, aCodecCtx,
						 &pFormatCtx->streams[base->audioStream]->time_base);
		else
			cmd = decoder_get_command(decoder);

		av_free_packet(&packet);

		if (cmd == DECODE_COMMAND_SEEK) {
			current = decoder_seek_where(decoder) * AV_TIME_BASE;

			if (av_seek_frame(pFormatCtx, -1, current, 0) < 0)
				decoder_seek_error(decoder);
			else
				decoder_command_finished(decoder);
		}
	} while (cmd != DECODE_COMMAND_STOP);

	return true;
}

static bool
ffmpeg_decode(struct decoder *decoder, struct input_stream *input)
{
	BasePtrs base;

	base.input = input;
	base.decoder = decoder;

	return ffmpeg_helper(input, ffmpeg_decode_internal, &base);
}

static bool ffmpeg_tag_internal(BasePtrs *base)
{
	struct tag *tag = (struct tag *) base->tag;

	if (base->pFormatCtx->duration != (int)AV_NOPTS_VALUE) {
		tag->time = base->pFormatCtx->duration / AV_TIME_BASE;
	} else {
		tag->time = 0;
	}

	return true;
}

//no tag reading in ffmpeg, check if playable
static struct tag *ffmpeg_tag(const char *file)
{
	struct input_stream input;
	BasePtrs base;
	bool ret;

	if (!input_stream_open(&input, file)) {
		ERROR("failed to open %s\n", file);
		return NULL;
	}

	base.decoder = NULL;
	base.tag = tag_new();

	ret = ffmpeg_helper(&input, ffmpeg_tag_internal, &base);
	if (ret) {
		tag_free(base.tag);
		base.tag = NULL;
	}

	input_stream_close(&input);

	return base.tag;
}

/**
 * ffmpeg can decode almost everything from open codecs
 * and also some of propietary codecs
 * its hard to tell what can ffmpeg decode
 * we can later put this into configure script
 * to be sure ffmpeg is used to handle
 * only that files
 */

static const char *const ffmpeg_Suffixes[] = {
	"wma", "asf", "wmv", "mpeg", "mpg", "avi", "vob", "mov", "qt", "swf", "rm", "swf",
	"mp1", "mp2", "mp3", "mp4", "m4a", "flac", "ogg", "wav", "au", "aiff", "aif", "ac3", "aac", "mpc",
	NULL
};

//not sure if this is correct...
static const char *const ffmpeg_Mimetypes[] = {
	"video/x-ms-asf",
	"audio/x-ms-wma",
	"audio/x-ms-wax",
	"video/x-ms-wmv",
	"video/x-ms-wvx",
	"video/x-ms-wm",
	"video/x-ms-wmx",
	"application/x-ms-wmz",
	"application/x-ms-wmd",
	"audio/mpeg",
	NULL
};

const struct decoder_plugin ffmpegPlugin = {
	.name = "ffmpeg",
	.init = ffmpeg_init,
	.try_decode = ffmpeg_try_decode,
	.stream_decode = ffmpeg_decode,
	.tag_dup = ffmpeg_tag,
	.stream_types = INPUT_PLUGIN_STREAM_URL | INPUT_PLUGIN_STREAM_FILE,
	.suffixes = ffmpeg_Suffixes,
	.mime_types = ffmpeg_Mimetypes
};
