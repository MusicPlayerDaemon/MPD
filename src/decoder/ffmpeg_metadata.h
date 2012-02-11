/*
 * Copyright (C) 2003-2012 The Music Player Daemon Project
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

#ifndef MPD_FFMPEG_METADATA_H
#define MPD_FFMPEG_METADATA_H

#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#if LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(51,5,0)
#include <libavutil/dict.h>
#endif

#if LIBAVFORMAT_VERSION_INT < AV_VERSION_INT(53,1,0)
#define AVDictionary AVMetadata
#define AVDictionaryEntry AVMetadataTag
#define av_dict_get av_metadata_get
#endif

struct tag_handler;

void
ffmpeg_scan_dictionary(AVDictionary *dict,
		       const struct tag_handler *handler, void *handler_ctx);

#endif
