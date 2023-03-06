// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_NORMALIZE_FILTER_PLUGIN_HXX
#define MPD_NORMALIZE_FILTER_PLUGIN_HXX

#include <memory>

struct FilterPlugin;
class PreparedFilter;

extern const FilterPlugin normalize_filter_plugin;

std::unique_ptr<PreparedFilter>
normalize_filter_prepare() noexcept;

#endif
