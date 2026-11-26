// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_FFMPEG_INPUT_PLUGIN_HXX
#define MPD_FFMPEG_INPUT_PLUGIN_HXX

#include "input/Ptr.hxx"
#include "thread/Mutex.hxx"

#include <string_view>

/**
 * An input plugin based on libavformat's "avio" library.
 */
extern const struct InputPlugin input_plugin_ffmpeg;

/**
 * Open an FFmpeg input stream with redirects disabled.
 */
[[nodiscard]]
InputStreamPtr
OpenFfmpegInputStreamNoRedirects(std::string_view uri, Mutex &mutex);

#endif
