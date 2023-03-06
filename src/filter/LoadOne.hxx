// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_FILTER_LOAD_ONE_HXX
#define MPD_FILTER_LOAD_ONE_HXX

#include <memory>

struct ConfigBlock;
class PreparedFilter;

/**
 * Creates a new filter, loads configuration and the plugin name from
 * the specified configuration section.
 *
 * Throws on error.
 *
 * @param block the configuration section
 */
std::unique_ptr<PreparedFilter>
filter_configured_new(const ConfigBlock &block);

#endif
