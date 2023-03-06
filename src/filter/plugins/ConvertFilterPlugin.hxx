// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_CONVERT_FILTER_PLUGIN_HXX
#define MPD_CONVERT_FILTER_PLUGIN_HXX

#include <memory>

class PreparedFilter;
class Filter;
struct AudioFormat;

std::unique_ptr<PreparedFilter>
convert_filter_prepare() noexcept;

std::unique_ptr<Filter>
convert_filter_new(AudioFormat in_audio_format,
		   AudioFormat out_audio_format);

/**
 * Sets the output audio format for the specified filter.  You must
 * call this after the filter has been opened.  Since this audio
 * format switch is a violation of the filter API, this filter must be
 * the last in a chain.
 *
 * Throws on error.
 */
void
convert_filter_set(Filter *filter, AudioFormat out_audio_format);

#endif
