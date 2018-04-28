/*
 * Copyright 2003-2017 The Music Player Daemon Project
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
#include "FfmpegMetaData.hxx"
#include "tag/Table.hxx"
#include "tag/Handler.hxx"
#include "tag/Id3MusicBrainz.hxx"
#include "util/Macros.hxx"

extern "C" {
#include <libavformat/avformat.h>
#include <libavutil/dict.h>
#include <libavcodec/avcodec.h>
}

static constexpr struct tag_table ffmpeg_tags[] = {
	{ "year", TAG_DATE },
	{ "author-sort", TAG_ARTIST_SORT },
	{ "album_artist", TAG_ALBUM_ARTIST },
	{ "album_artist-sort", TAG_ALBUM_ARTIST_SORT },

	/* sentinel */
	{ nullptr, TAG_NUM_OF_ITEM_TYPES }
};

static void
FfmpegScanTag(TagType type,
	      AVDictionary *m, const char *name,
	      const TagHandler &handler, void *handler_ctx)
{
	AVDictionaryEntry *mt = nullptr;

	while ((mt = av_dict_get(m, name, mt, 0)) != nullptr)
		tag_handler_invoke_tag(handler, handler_ctx,
				       type, mt->value);
}

static void
FfmpegScanPairs(AVDictionary *dict,
		const TagHandler &handler, void *handler_ctx)
{
	AVDictionaryEntry *i = nullptr;

	while ((i = av_dict_get(dict, "", i, AV_DICT_IGNORE_SUFFIX)) != nullptr)
		tag_handler_invoke_pair(handler, handler_ctx,
					i->key, i->value);
}

void
FfmpegScanDictionary(AVDictionary *dict,
		     const TagHandler &handler, void *handler_ctx)
{
	if (handler.tag != nullptr) {
		for (unsigned i = 0; i < TAG_NUM_OF_ITEM_TYPES; ++i)
			FfmpegScanTag(TagType(i), dict, tag_item_names[i],
				      handler, handler_ctx);

		for (const struct tag_table *i = ffmpeg_tags;
		     i->name != nullptr; ++i)
			FfmpegScanTag(i->type, dict, i->name,
				      handler, handler_ctx);

		for (const struct tag_table *i = musicbrainz_txxx_tags;
		     i->name != nullptr; ++i)
			FfmpegScanTag(i->type, dict, i->name,
				      handler, handler_ctx);
	}

	if (handler.pair != nullptr)
		FfmpegScanPairs(dict, handler, handler_ctx);
}

static const char *mime_tympename[] = {
	"image/jpeg",
	"image/png",
	"image/x-ms-bmp",
	"image/jp2",
	"image/x-portable-pixmap",
	"image/gif",
	"image/x-pcx",
	"image/x-targa image/x-tga",
	"image/tiff",
	"image/webp",
	"image/x-xwindowdump",
};

enum MimeNameIndex
{
	MJPEG,
	PNG,
	BMP,
	JPEG2000,
	PAM,
	GIF,
	PCX,
	TARGA,
	TIFF,
	WEBP,
	XWD,
};

static int mime_tbl[] = {
	AV_CODEC_ID_MJPEG,
	AV_CODEC_ID_PNG,
	AV_CODEC_ID_BMP,
	AV_CODEC_ID_JPEG2000,
	AV_CODEC_ID_PAM,
	AV_CODEC_ID_GIF,
	AV_CODEC_ID_PCX,
	AV_CODEC_ID_TARGA,
	AV_CODEC_ID_TIFF,
	AV_CODEC_ID_WEBP,
	AV_CODEC_ID_XWD,
};

static const char  *
get_MIMEDescriptor(enum AVCodecID id)
{
	if (id < ARRAY_SIZE(mime_tbl)) {
		for (unsigned i=0;i<ARRAY_SIZE(mime_tbl);i++) {
			if (mime_tbl[i] == id) {
				return mime_tympename[i];
			}
		}
	}

	return nullptr;
}

static bool
ffmpeg_copy_cover_paramer(CoverType type, unsigned value,
						 const TagHandler &handler, void *handler_ctx)
{
	char buf[21];

	if (snprintf(buf, sizeof(buf), "%u", value)) {
		tag_handler_invoke_cover(handler, handler_ctx, type, (const char*)buf);
		return true;
	}

	return false;
}


void
FfmpegScanCover(AVFormatContext &format_context,
    const TagHandler &handler, void *handler_ctx)
{
	assert(handler.cover != nullptr);

	for (unsigned i=0; i<format_context.nb_streams; ++i) {
		if(format_context.streams[i]->disposition & AV_DISPOSITION_ATTACHED_PIC) {
			AVPacket pkt = format_context.streams[i]->attached_pic;
			if (pkt.data != nullptr) {
				ffmpeg_copy_cover_paramer(COVER_TYPE, 0, handler, handler_ctx);
				const char  * mime_types = get_MIMEDescriptor(format_context.streams[i]->codec->codec_id);
				if (mime_types != nullptr) {
					tag_handler_invoke_cover(handler, handler_ctx, COVER_MIME,mime_types);
				}
				ffmpeg_copy_cover_paramer(COVER_WIDTH, format_context.streams[i]->codec->width, handler, handler_ctx);
				ffmpeg_copy_cover_paramer(COVER_HEIGHT, format_context.streams[i]->codec->height, handler, handler_ctx);
				ffmpeg_copy_cover_paramer(COVER_LENGTH, pkt.size, handler, handler_ctx);
				tag_handler_invoke_cover(handler, handler_ctx, COVER_DATA, (const char*)pkt.data, pkt.size);
				return;
			}
		}
	}
}
