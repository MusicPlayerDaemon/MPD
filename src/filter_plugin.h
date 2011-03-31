/*
 * Copyright (C) 2003-2011 The Music Player Daemon Project
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

#ifndef MPD_FILTER_PLUGIN_H
#define MPD_FILTER_PLUGIN_H

#include <glib.h>

#include <stdbool.h>
#include <stddef.h>

struct config_param;
struct filter;

struct filter_plugin {
	const char *name;

	/**
         * Allocates and configures a filter.
	 */
	struct filter *(*init)(const struct config_param *param,
			       GError **error_r);

	/**
	 * Free instance data.
         */
	void (*finish)(struct filter *filter);

	/**
	 * Opens a filter.
	 *
	 * @param audio_format the audio format of incoming data; the
	 * plugin may modify the object to enforce another input
	 * format
	 */
	const struct audio_format *
	(*open)(struct filter *filter,
		struct audio_format *audio_format,
		GError **error_r);

	/**
	 * Closes a filter.
	 */
	void (*close)(struct filter *filter);

	/**
	 * Filters a block of PCM data.
	 */
	const void *(*filter)(struct filter *filter,
			      const void *src, size_t src_size,
			      size_t *dest_buffer_r,
			      GError **error_r);
};

/**
 * Creates a new instance of the specified filter plugin.
 *
 * @param plugin the filter plugin
 * @param param optional configuration section
 * @param error location to store the error occurring, or NULL to
 * ignore errors.
 * @return a new filter object, or NULL on error
 */
struct filter *
filter_new(const struct filter_plugin *plugin,
	   const struct config_param *param, GError **error_r);

/**
 * Creates a new filter, loads configuration and the plugin name from
 * the specified configuration section.
 *
 * @param param the configuration section
 * @param error location to store the error occurring, or NULL to
 * ignore errors.
 * @return a new filter object, or NULL on error
 */
struct filter *
filter_configured_new(const struct config_param *param, GError **error_r);

/**
 * Deletes a filter.  It must be closed prior to calling this
 * function, see filter_close().
 *
 * @param filter the filter object
 */
void
filter_free(struct filter *filter);

/**
 * Opens the filter, preparing it for filter_filter().
 *
 * @param filter the filter object
 * @param audio_format the audio format of incoming data; the plugin
 * may modify the object to enforce another input format
 * @param error location to store the error occurring, or NULL to
 * ignore errors.
 * @return the format of outgoing data
 */
const struct audio_format *
filter_open(struct filter *filter, struct audio_format *audio_format,
	    GError **error_r);

/**
 * Closes the filter.  After that, you may call filter_open() again.
 *
 * @param filter the filter object
 */
void
filter_close(struct filter *filter);

/**
 * Filters a block of PCM data.
 *
 * @param filter the filter object
 * @param src the input buffer
 * @param src_size the size of #src_buffer in bytes
 * @param dest_size_r the size of the returned buffer
 * @param error location to store the error occurring, or NULL to
 * ignore errors.
 * @return the destination buffer on success (will be invalidated by
 * filter_close() or filter_filter()), NULL on error
 */
const void *
filter_filter(struct filter *filter, const void *src, size_t src_size,
	      size_t *dest_size_r,
	      GError **error_r);

#endif
