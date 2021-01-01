/*
 * Copyright 2003-2021 The Music Player Daemon Project
 * http://www.musicpd.org
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

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
