/*
 * Copyright 2003-2016 The Music Player Daemon Project
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

/** \file
 *
 * This header declares the filter_plugin class.  It describes a
 * plugin API for objects which filter raw PCM data.
 */

#ifndef MPD_FILTER_PLUGIN_HXX
#define MPD_FILTER_PLUGIN_HXX

struct ConfigBlock;
class Filter;
class Error;

struct filter_plugin {
	const char *name;

	/**
         * Allocates and configures a filter.
	 */
	Filter *(*init)(const ConfigBlock &block, Error &error);
};

/**
 * Creates a new instance of the specified filter plugin.
 *
 * @param plugin the filter plugin
 * @param block configuration section
 * @param error location to store the error occurring, or nullptr to
 * ignore errors.
 * @return a new filter object, or nullptr on error
 */
Filter *
filter_new(const struct filter_plugin *plugin,
	   const ConfigBlock &block, Error &error);

/**
 * Creates a new filter, loads configuration and the plugin name from
 * the specified configuration section.
 *
 * @param block the configuration section
 * @param error location to store the error occurring, or nullptr to
 * ignore errors.
 * @return a new filter object, or nullptr on error
 */
Filter *
filter_configured_new(const ConfigBlock &block, Error &error);

#endif
