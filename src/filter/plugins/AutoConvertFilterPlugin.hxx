// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_AUTOCONVERT_FILTER_PLUGIN_HXX
#define MPD_AUTOCONVERT_FILTER_PLUGIN_HXX

#include <memory>

class PreparedFilter;

/**
 * Creates a new "autoconvert" filter.  When opened, it ensures that
 * the input audio format isn't changed.  If the underlying filter
 * requests a different format, it automatically creates a
 * convert_filter.
 */
std::unique_ptr<PreparedFilter>
autoconvert_filter_new(std::unique_ptr<PreparedFilter> filter) noexcept;

#endif
