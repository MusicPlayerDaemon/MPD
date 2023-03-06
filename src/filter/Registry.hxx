// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

/** \file
 *
 * This library manages all filter plugins which are enabled at
 * compile time.
 */

#ifndef MPD_FILTER_REGISTRY_HXX
#define MPD_FILTER_REGISTRY_HXX

struct FilterPlugin;

[[gnu::pure]]
const FilterPlugin *
filter_plugin_by_name(const char *name) noexcept;

#endif
