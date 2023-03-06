// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_PLUGIN_UNAVAILABLE_HXX
#define MPD_PLUGIN_UNAVAILABLE_HXX

#include <stdexcept>

/**
 * An exception class which is used by plugin initializers to indicate
 * that this plugin is unavailable.  It will be disabled, and MPD can
 * continue initialization.
 */
class PluginUnavailable : public std::runtime_error {
public:
	using std::runtime_error::runtime_error;
};

/**
 * Like #PluginUnavailable, but denotes that the plugin is not
 * available because it was not explicitly enabled in the
 * configuration.  The message may describe the necessary steps to
 * enable it.
 */
class PluginUnconfigured : public PluginUnavailable {
public:
	using PluginUnavailable::PluginUnavailable;
};

#endif
