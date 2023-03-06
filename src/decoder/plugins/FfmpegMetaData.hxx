// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_FFMPEG_METADATA_HXX
#define MPD_FFMPEG_METADATA_HXX

struct AVDictionary;
class TagHandler;

void
FfmpegScanDictionary(AVDictionary *dict, TagHandler &handler) noexcept;

#endif
