// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

/** \file
 *
 * This header declares the filter_plugin class.  It describes a
 * plugin API for objects which filter raw PCM data.
 */

#ifndef MPD_FILTER_PLUGIN_HXX
#define MPD_FILTER_PLUGIN_HXX

#include <memory>

struct ConfigBlock;
class PreparedFilter;

struct FilterPlugin {
	const char *name;

	/**
         * Allocates and configures a filter.
	 */
	std::unique_ptr<PreparedFilter> (*init)(const ConfigBlock &block);
};

#endif
