// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_VOLUME_FILTER_PLUGIN_HXX
#define MPD_VOLUME_FILTER_PLUGIN_HXX

#include <memory>

class PreparedFilter;
class Filter;

std::unique_ptr<PreparedFilter>
volume_filter_prepare() noexcept;

unsigned
volume_filter_get(const Filter *filter) noexcept;

void
volume_filter_set(Filter *filter, unsigned volume) noexcept;

#endif
